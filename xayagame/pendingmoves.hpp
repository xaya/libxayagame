// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_PENDINGMOVES_HPP
#define XAYAGAME_PENDINGMOVES_HPP

#include "gamelogic.hpp"
#include "storage.hpp"

#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <memory>
#include <map>

namespace xaya
{

/**
 * Processor for pending moves in the game.  This can be subclassed with
 * actual logic (and storage of data) as needed by games.  They can then
 * implement whatever processing they need to keep track of a "pending state"
 * based on the current mempool.
 */
class PendingMoveProcessor : public GameProcessorWithContext
{

private:

  /**
   * Data about the "current state" accessible to the callbacks while they
   * are being executed.
   */
  struct CurrentState
  {

    /** The current confirmed game state.  */
    const GameStateData& state;

    explicit CurrentState (const GameStateData& s)
      : state(s)
    {}

  };

  /**
   * All currently known pending moves, indexed by their txid.  This is used
   * to check whether a new move is already known, and also to retrieve the
   * actual data when we sync with getrawmempool.
   */
  std::map<uint256, Json::Value> pending;

  /** While a callback is running, the state context.  */
  std::unique_ptr<CurrentState> ctx;

  /**
   * Resets the internal state, by clearing and then rebuilding from the
   * list of pending moves, and syncing them with getrawmempool.  This assumes
   * that the state context is already set up.
   */
  void Reset ();

  class ContextSetter;

protected:

  /**
   * Returns the currently confirmed on-chain game state.  This must only
   * be called while a callback (Clear or AddPendingMove) is currently
   * running.
   */
  const GameStateData& GetConfirmedState () const;

  /**
   * Clears the state, so it corresponds to an empty mempool.
   */
  virtual void Clear () = 0;

  /**
   * Adds a new pending move to the current pending state in this instance.
   * mv contains the full move data as JSON.
   *
   * Between calls to Clear, this is called at most once for any particular
   * transaction.  If one move is built on another (i.e. spending the other's
   * name), then it is usually passed to AddPendingMove later.
   *
   * During exceptional situations (e.g. reorgs), it may happen that
   * conflicting, out-of-order or already confirmed moves are passed here.
   * Implementations must be able to handle that gracefully, although the
   * resulting pending state may be "wrong".
   */
  virtual void AddPendingMove (const Json::Value& mv) = 0;

public:

  PendingMoveProcessor () = default;

  /**
   * Processes a newly attached block.  This checks the current mempool
   * of Xaya Core and then rebuilds the pending state based on known moves
   * that are still in the mempool.
   */
  void ProcessAttachedBlock (const GameStateData& state);

  /**
   * Processes a detached block.  This clears the pending state and rebuilds
   * it from Xaya Core's mempool (including re-added transactions from
   * the block that was just detached).
   *
   * state must be the confirmed game-state *after* the block has been
   * detached already (i.e. the state before, not "at", the block).
   */
  void ProcessDetachedBlock (const GameStateData& state,
                             const Json::Value& blockData);

  /**
   * Processes a newly received pending move.
   */
  void ProcessMove (const GameStateData& state, const Json::Value& mv);

  /**
   * Returns a JSON representation of the current state.  This is exposed
   * by the GSP's RPC server for use by frontends (and the likes).
   */
  virtual Json::Value ToJson () const = 0;

};

} // namespace xaya

#endif // XAYAGAME_PENDINGMOVES_HPP
