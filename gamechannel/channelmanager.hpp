// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELMANAGER_HPP
#define GAMECHANNEL_CHANNELMANAGER_HPP

#include "boardrules.hpp"
#include "broadcast.hpp"
#include "movesender.hpp"
#include "openchannel.hpp"
#include "rollingstate.hpp"

#include "proto/stateproof.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace xaya
{

/**
 * The main logic for a channel daemon.  This class keeps track of the
 * state (except for game-specific pieces of data, of course), including
 * the actual board states known but also information about disputes.
 * It updates the states as moves and on-chain updates come in, provides
 * functions to query the state (used by the RPC server) and can request
 * resolutions if disputes are filed against the player and a newer state
 * is already known.
 *
 * This class performs locking as needed, and its functions (e.g. updates
 * for on-chain and off-chain changes) may be freely called from different
 * threads and in parallel.
 */
class ChannelManager
{

private:

  /**
   * Data stored about a potential dispute on the current channel.
   */
  struct DisputeData
  {

    /** The block height at which the dispute is filed.  */
    unsigned height;

    /** The player whose turn it is at the dispute.  */
    int turn;

    /** The turn count at which the disputed state is.  */
    unsigned count;

    /**
     * True if we already tried to send a resolution for the last known
     * on-chain block.
     */
    bool pendingResolution;

    DisputeData () = default;
    DisputeData (const DisputeData&) = default;
    DisputeData& operator= (const DisputeData&) = default;

  };

  /**
   * Mutex protecting the state in this class.  This is needed since there
   * may be multiple threads calling functions on the ChannelManager (e.g.
   * one thread listing to the GSP's waitforchange and another on the real-time
   * broadcast network).
   *
   * This is also used as lock for the waitforchange condition variable.
   */
  mutable std::mutex mut;

  /**
   * Condition variable that gets signalled when the state is changed due
   * to on-chain updates, off-chain updates or local moves.  This is used
   * for waitforchange.
   */
  mutable std::condition_variable cvStateChanged;

  /** The board rules of the game being played.  */
  const BoardRules& rules;

  /** OpenChannel instance for this game.  */
  OpenChannel& game;

  /** RPC connection to Xaya Core used for verifying signatures.  */
  XayaRpcClient& rpc;

  /** RPC connection to the Xaya wallet used for signing local moves.  */
  XayaWalletRpcClient& wallet;

  /** The ID of the managed channel.  */
  const uint256 channelId;

  /**
   * The Xaya name that corresponds to the player that is using the
   * current channel daemon (without p/ prefix).
   */
  const std::string playerName;

  /** Data about the board states we know.  */
  RollingState boardStates;

  /**
   * Broadcaster for off-chain moves.  This must be initialised before any
   * functions are called that would trigger a broadcast.
   */
  OffChainBroadcast* offChainSender = nullptr;

  /**
   * Instance for sending on-chain moves (disputes / resolutions).  This must
   * be set before any functions may be called that trigger such moves.
   */
  MoveSender* onChainSender = nullptr;

  /**
   * Version counter for the current state.  Whenever the state is changed,
   * this value is incremented.  It can be used to identify a certain state,
   * so that callers of WaitForChange can tell it which state they already
   * know and get notified on changes from that state.
   */
  int stateVersion = 1;

  /**
   * If set to true, then no more updates will be processed.  This is used
   * when shutting down the channel daemon and to make sure that we can properly
   * handle waking up all waitforchange callers (without them re-calling
   * and blocking again).
   */
  bool stopped = false;

  /**
   * If set to false, it means that there is no on-chain data about the
   * channel ID.  This may be the case because the channel creation has not
   * been confirmed yet, or perhaps because the channel is already closed.
   */
  bool exists = false;

  /** Data about an open dispute, if any.  */
  std::unique_ptr<DisputeData> dispute;

  /**
   * Set to true if we already tried to file a pending dispute for the last
   * known block height.
   */
  bool pendingDispute = false;

  /**
   * Tries to apply a local move to the current state.  Returns true if
   * a change was made successfully.  This method just updates the
   * state, without triggering any more processing by itself.  It is
   * the shared code between ProcessLocalMove and processing of automoves.
   */
  bool ApplyLocalMove (const BoardMove& mv);

  /**
   * Tries to resolve the current dispute, if there is any.  This can be called
   * whenever a change may have happened that affects this, like a new state
   * being known (e.g. off-chain / local move) or an on-chain update.
   */
  void TryResolveDispute ();

  /**
   * Tries to apply a chain of automoves to the current state, if applicable.
   * Returns true if at least one move was found.
   */
  bool ProcessAutoMoves ();

  /**
   * Performs internal updates after the state was changed.  In particular,
   * this performs automoves, resolves disputes and notifies the OpenChannel
   * and WaitForChange listeners about a new change.
   *
   * If automoves were found or broadcast is true, then it also broadcasts
   * the new state to the off-chain channel.
   */
  void ProcessStateUpdate (bool broadcast);

  /**
   * Returns the current state of the channel as JSON, assuming that mut is
   * already locked.  This is used internally for ToJson as well as
   * WaitForChange (the latter cannot call the former directly, since that would
   * lock the mutex twice).
   */
  Json::Value UnlockedToJson () const;

  /**
   * Notifies threads waiting on cvStateChanged that a new state is available.
   * This also increments the state version.
   */
  void NotifyStateChange ();

  friend class ChannelManagerTestFixture;

public:

  /**
   * Special value for the known version in WaitForChange that tells the
   * function to always block.
   */
  static constexpr int WAITFORCHANGE_ALWAYS_BLOCK = 0;

  explicit ChannelManager (const BoardRules& r, OpenChannel& oc,
                           XayaRpcClient& c, XayaWalletRpcClient& w,
                           const uint256& id, const std::string& name);

  ~ChannelManager ();

  ChannelManager () = delete;
  ChannelManager (const ChannelManager&) = delete;
  void operator= (const ChannelManager&) = delete;

  void SetOffChainBroadcast (OffChainBroadcast& s);
  void SetMoveSender (MoveSender& s);

  const uint256&
  GetChannelId () const
  {
    return channelId;
  }

  /**
   * Processes a (potentially) new move retrieved through the off-chain
   * broadcasting network.
   */
  void ProcessOffChain (const std::string& reinitId,
                        const proto::StateProof& proof);

  /**
   * Processes an on-chain update that did not contain any data for our channel.
   */
  void ProcessOnChainNonExistant ();

  /**
   * Processes a (potentially) new on-chain state for the channel.
   */
  void ProcessOnChain (const proto::ChannelMetadata& meta,
                       const BoardState& reinitState,
                       const proto::StateProof& proof,
                       unsigned disputeHeight);

  /**
   * Processes a move made locally, i.e. by the player who runs the channel
   * manager.  This tries to apply the move to the current state, sign the
   * resulting state, build a new state proof, and then broadcast it.
   */
  void ProcessLocalMove (const BoardMove& mv);

  /**
   * Requests to file a dispute with the current state.
   */
  void FileDispute ();

  /**
   * Disables processing of updates in the future.  This should be called
   * when shutting down the channel daemon.  It makes sure that all waiting
   * callers to WaitForChange are woken up, and no more callers will block
   * in the future.  Thus, this mechanism ensures that we can properly
   * shut down WaitForChange.
   *
   * This function must be called before a ChannelManager instance is
   * destructed.  Otherwise the destructor will CHECK-fail.
   */
  void StopUpdates ();

  /**
   * Returns the current state of this channel as JSON, suitable to be
   * sent to frontends.
   */
  Json::Value ToJson () const;

  /**
   * Blocks the calling thread until the state of the channel has (probably)
   * been changed.  This can be used by frontends to implement long-polling
   * RPC methods like waitforchange.  Note that the function may return
   * spuriously even if there is no new state.
   *
   * If the passed-in version is different from the current state version
   * already when starting the call, the function returns immediately.  Ideally,
   * clients should pass in the version they currently know (as returned
   * in the JSON state in "version"), so that we can avoid race conditions
   * when a change happens between two calls to WaitForChange.
   *
   * When WAITFORCHANGE_ALWAYS_BLOCK is passed as the known version, then the
   * function will always block until the next update.
   *
   * On return, the current (i.e. likely new) state is returned in the same
   * format as ToJson() would return.
   */
  Json::Value WaitForChange (int knownVersion) const;

};

} // namespace xaya

#endif // GAMECHANNEL_CHANNELMANAGER_HPP
