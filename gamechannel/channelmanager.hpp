// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELMANAGER_HPP
#define GAMECHANNEL_CHANNELMANAGER_HPP

#include "boardrules.hpp"
#include "movesender.hpp"
#include "rollingstate.hpp"

#include "proto/stateproof.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayautil/uint256.hpp>

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
   */
  mutable std::mutex mut;

  /** The board rules of the game being played.  */
  const BoardRules& rules;

  /** RPC connection to Xaya Core used for verifying signatures.  */
  XayaRpcClient& rpc;

  /** The ID of the managed channel.  */
  const uint256 channelId;

  /**
   * The Xaya name that corresponds to the player that is using the
   * current channel daemon.
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
   * Tries to resolve the current dispute, if there is any.  This can be called
   * whenever a change may have happened that affects this, like a new state
   * being known (e.g. off-chain / local move) or an on-chain update.
   */
  void TryResolveDispute ();

  friend class ChannelManagerTests;

public:

  explicit ChannelManager (const BoardRules& r, XayaRpcClient& c,
                           const uint256& id, const std::string& name);

  ChannelManager () = delete;
  ChannelManager (const ChannelManager&) = delete;
  void operator= (const ChannelManager&) = delete;

  void SetOffChainBroadcast (OffChainBroadcast& s);
  void SetMoveSender (MoveSender& s);

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
   * Requests to file a dispute with the current state.
   */
  void FileDispute ();

};

} // namespace xaya

#endif // GAMECHANNEL_CHANNELMANAGER_HPP
