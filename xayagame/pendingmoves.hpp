// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_PENDINGMOVES_HPP
#define XAYAGAME_PENDINGMOVES_HPP

#include "gamelogic.hpp"
#include "storage.hpp"

#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <deque>
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

    /**
     * The last confirmed block's meta data as per the Xaya Core ZMQ
     * notifications, namely the "block" field.
     */
    const Json::Value& block;

    explicit CurrentState (const GameStateData& s, const Json::Value& blk)
      : state(s), block(blk)
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
   * In order to know the current confirmed block metadata, we keep track of
   * a list of the last attached blocks here.  After a certain number of blocks
   * are in the queue, we keep dropping the oldest ones.
   *
   * In theory it might happen that this queue runs empty, e.g. if a long
   * reorg happens and/or immediately after starting up.  In this case, we
   * simply stop processing pending moves temporarily (which is fine as it
   * will happen only very rarely and is not consensus-relevant anyway).
   */
  std::deque<Json::Value> blockQueue;

  /**
   * Resets the internal state, by clearing and then rebuilding from the
   * list of pending moves, and syncing them with getrawmempool.  This sets
   * up the state context for the given game state and using our blockQueue.
   */
  void Reset (const GameStateData& state);

  class ContextSetter;

protected:

  /**
   * Returns the currently confirmed on-chain game state.  This must only
   * be called while AddPendingMove is currently running.
   */
  const GameStateData& GetConfirmedState () const;

  /**
   * Returns the JSON data of the last confirmed block as per the ZMQ
   * notifications.  This contains the notification "block" field, i.e.
   * the block metadata like height or timestamp.
   *
   * The function must only be called wile AddPendingMove is running.
   */
  const Json::Value& GetConfirmedBlock () const;

  /**
   * Clears the state, so it corresponds to an empty mempool.  This is called
   * whenever the confirmed on-chain state changes.  It may also be called
   * when the confirmed state did not change but the pending state needs to
   * be rebuilt due to some other reason.
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
  void ProcessAttachedBlock (const GameStateData& state,
                             const Json::Value& blockData);

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
