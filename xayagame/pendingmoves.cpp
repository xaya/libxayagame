// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pendingmoves.hpp"

#include <glog/logging.h>

namespace xaya
{

namespace
{

/** Size of the in-memory block queue that is kept.  */
constexpr size_t BLOCK_QUEUE_SIZE = 100;

} // anonymous namespace

/**
 * Helper class to set/unset the current-state context in a PendingMoveProcessor
 * using RAII.
 */
class PendingMoveProcessor::ContextSetter
{

private:

  /** The corresponding PendingMoveProcessor instance.  */
  PendingMoveProcessor& proc;

public:

  /**
   * Sets the context based on the current state.
   */
  explicit ContextSetter (PendingMoveProcessor& p, const GameStateData& s,
                          const Json::Value& blk)
    : proc(p)
  {
    CHECK (proc.ctx == nullptr);
    CHECK (blk.isObject ());
    proc.ctx = std::make_unique<CurrentState> (s, blk);
  }

  /**
   * Unsets the context.
   */
  ~ContextSetter ()
  {
    CHECK (proc.ctx != nullptr);
    proc.ctx.reset ();
  }

};

const GameStateData&
PendingMoveProcessor::GetConfirmedState () const
{
  CHECK (ctx != nullptr) << "No callback is running at the moment";
  return ctx->state;
}

const Json::Value&
PendingMoveProcessor::GetConfirmedBlock () const
{
  CHECK (ctx != nullptr) << "No callback is running at the moment";
  return ctx->block;
}

void
PendingMoveProcessor::Reset (const GameStateData& state)
{
  const auto mempool = GetXayaRpc ().getrawmempool ();
  VLOG (1)
      << "Rebuilding pending move state with " << mempool.size ()
      << " transactions in the (full) mempool...";

  /* We clear the state in any case, even if the blockQueue is empty.  This is
     fine, as Clear is not supposed to have a context anyway.  And it will
     ensure that we get at least an empty state if we can't process pending
     moves due to the blockQueue being empty.  */
  Clear ();

  /* If we do have a block queue, set up a context and use it (later) to
     process pending moves.  */
  std::unique_ptr<ContextSetter> setter;
  if (blockQueue.empty ())
    LOG (WARNING) << "Block queue is empty, ignoring pending moves for now";
  else
    setter = std::make_unique<ContextSetter> (*this, state, blockQueue.back ());

  std::map<uint256, Json::Value> newPending;
  for (const auto& txidStr : mempool)
    {
      uint256 txid;
      CHECK (txidStr.isString ());
      CHECK (txid.FromHex (txidStr.asString ()));

      const auto mit = pending.find (txid);
      if (mit == pending.end ())
        continue;

      newPending.emplace (txid, mit->second);
      if (ctx != nullptr)
        AddPendingMove (mit->second);
    }

  VLOG (1)
      << "Sync with real mempool reduced size of pending moves from "
      << pending.size () << " to " << newPending.size ();
  pending = std::move (newPending);
}

namespace
{

/**
 * Extracts the txid of a move JSON object as uint256.
 */
uint256
GetMoveTxid (const Json::Value& mv)
{
  const auto& txidVal = mv["txid"];
  CHECK (txidVal.isString ());

  uint256 txid;
  CHECK (txid.FromHex (txidVal.asString ()));

  return txid;
}

} // anonymous namespace

void
PendingMoveProcessor::ProcessAttachedBlock (const GameStateData& state,
                                            const Json::Value& blockData)
{
  VLOG (1) << "Updating pending state for attached block...";

  const auto& data = blockData["block"];
  CHECK (data.isObject ());

  /* Game does not call ProcessAttachedBlock for every block it receives,
     e.g. not during catching-up phase.  Thus we cannot assume that we can
     keep track of an accurate block queue at all times.  If there is a
     mismatch with the new block, make sure to clear the bad data.  */
  if (!blockQueue.empty () && blockQueue.back ()["hash"] != data["parent"])
    {
      LOG (WARNING) << "Bad block queue detected, clearing out";
      blockQueue.clear ();
    }

  blockQueue.push_back (data);
  while (blockQueue.size () > BLOCK_QUEUE_SIZE)
    blockQueue.pop_front ();

  Reset (state);
}

void
PendingMoveProcessor::ProcessDetachedBlock (const GameStateData& state,
                                            const Json::Value& blockData)
{
  /* We want to insert moves from the detached block into our map of
     known moves, so that we can process them in case they are later on still
     in Xaya Core's mempool.  */
  const auto& mvArray = blockData["moves"];
  CHECK (mvArray.isArray ());
  for (const auto& mv : mvArray)
    {
      const uint256 txid = GetMoveTxid (mv);
      pending.emplace (txid, mv);
    }

  VLOG (1)
      << "Updating pending state for detached block "
      << blockData["block"]["hash"].asString () << ": "
      << mvArray.size () << " moves unconfirmed";
  VLOG (2) << "Block data: " << blockData;

  /* It is not guaranteed that we receive all attach/detach callbacks from
     Game.  Thus it may happen that we have inconsistent data, in which case
     we just clear it.  */
  if (!blockQueue.empty ())
    {
      if (blockQueue.back () == blockData["block"])
        blockQueue.pop_back ();
      else
        {
          LOG (WARNING) << "Bad block queue detected, clearing out";
          blockQueue.clear ();
        }
    }

  Reset (state);
}

void
PendingMoveProcessor::ProcessMove (const GameStateData& state,
                                   const Json::Value& mv)
{
  const uint256 txid = GetMoveTxid (mv);
  VLOG (1) << "Processing pending move: " << txid.ToHex ();
  VLOG (2) << "Full data: " << mv;

  const auto inserted = pending.emplace (txid, mv);
  if (!inserted.second)
    {
      VLOG (1) << "The move is already known";
      return;
    }

  if (blockQueue.empty ())
    LOG (WARNING) << "Block queue is empty, ignoring pending move for now";
  else
    {
      ContextSetter setter(*this, state, blockQueue.back ());
      AddPendingMove (mv);
    }
}

} // namespace xaya
