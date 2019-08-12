// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHAINTOCHANNEL_HPP
#define GAMECHANNEL_CHAINTOCHANNEL_HPP

#include "channelmanager.hpp"

#include "rpc-stubs/channelgsprpcclient.h"

#include <memory>
#include <atomic>
#include <thread>

namespace xaya
{

/**
 * Instances of this class connect to a channel-game GSP by RPC and feed
 * updates received for a particular channel to a local ChannelManager.
 * This is done through separate threads that just call waitforchange,
 * getchannel and waitforpendingchange in a loop.
 */
class ChainToChannelFeeder
{

private:

  /** The RPC connection for waitforchange.  */
  ChannelGspRpcClient& rpcBlocks;
  /**
   * The RPC connection for waitforpendingchange.  We need a separate instance
   * here since this is called from a different thread than waitforchange.
   *
   * This may be null, in which case no pending moves are processed.
   */
  ChannelGspRpcClient* rpcPending;

  /** The ChannelManager that is updated.  */
  ChannelManager& manager;

  /** Our channel ID as hex (used to process updates).  */
  const std::string channelIdHex;

  /** The running thread (if any) for block updates.  */
  std::unique_ptr<std::thread> loopBlocks;
  /** The running thread (if any) for pending updates.  */
  std::unique_ptr<std::thread> loopPending;

  /** Atomic flag telling the running threads to stop.  */
  std::atomic<bool> stopLoop;

  /**
   * The last block hash to which we updated the channel manager.  This is
   * only accessed from the loop thread (and the constructor).
   */
  uint256 lastBlock;

  /**
   * The last "version" of the pending state returned.  This is only accessed
   * from the loop thread doing pending updates.
   */
  int pendingVersion = 0;

  /**
   * The last base64-encoded StateProof received in a pending move.  We use
   * this as a crude test to detect when a pending update actually changed
   * the state of our channel, so that we avoid feeding unnecessary updates
   * to the ChannelManager.
   */
  std::string lastPendingProof;

  /**
   * Queries the GSP for the current state and updates the ChannelManager
   * and lastBlock from the result.
   */
  void UpdateBlocks ();

  /**
   * Performs an update of the pending state (i.e. potentially feeds the
   * new StateProof to our ChannelManager) based on the given JSON.
   */
  void UpdatePending (const Json::Value& state);

  /**
   * Runs the main loop for block updates.
   */
  void RunBlockLoop ();

  /**
   * Runs the main loop for pending updates.
   */
  void RunPendingLoop ();

public:

  /**
   * Constructs a ChainToChannelFeeder instance based on the given GSP RPC
   * client(s) and ChannelManager to update.  Note that the GSP RPC clients will
   * be used from separate threads and must thus not be used anywhere else
   * at the same time, nor can they be the same instance.
   *
   * If no RPC client for pending updates is given, then processing of
   * pending moves is disabled.
   */
  explicit ChainToChannelFeeder (ChannelGspRpcClient& rBlocks,
                                 ChannelGspRpcClient* rPending,
                                 ChannelManager& cm);

  ~ChainToChannelFeeder ();

  ChainToChannelFeeder () = delete;
  ChainToChannelFeeder (const ChainToChannelFeeder&) = delete;
  void operator= (const ChainToChannelFeeder&) = delete;

  /**
   * Starts the main loop in a separate thread.
   */
  void Start ();

  /**
   * Stops the main loop.  This is automatically called in the destructor
   * if the loop is still running then.
   *
   * Note that this has to wait for the current waitforchange call to return,
   * which may require it to time out.
   */
  void Stop ();

};

} // namespace xaya

#endif // GAMECHANNEL_CHAINTOCHANNEL_HPP
