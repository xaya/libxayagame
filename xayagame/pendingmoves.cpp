// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pendingmoves.hpp"

#include <glog/logging.h>

namespace xaya
{

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
  explicit ContextSetter (PendingMoveProcessor& p, const GameStateData& s)
    : proc(p)
  {
    CHECK (proc.ctx == nullptr);
    proc.ctx = std::make_unique<CurrentState> (s);
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

void
PendingMoveProcessor::Reset ()
{
  const auto mempool = GetXayaRpc ().getrawmempool ();
  VLOG (1)
      << "Rebuilding pending move state with " << mempool.size ()
      << " transactions in the (full) mempool...";

  Clear ();
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
PendingMoveProcessor::ProcessAttachedBlock (const GameStateData& state)
{
  VLOG (1) << "Updating pending state for attached block...";

  ContextSetter setter(*this, state);
  Reset ();
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

  ContextSetter setter(*this, state);
  Reset ();
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

  ContextSetter setter(*this, state);
  AddPendingMove (mv);
}

} // namespace xaya
