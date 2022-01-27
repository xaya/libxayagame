// Copyright (C) 2018-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHAINTOCHANNEL_HPP
#define GAMECHANNEL_CHAINTOCHANNEL_HPP

#include "syncmanager.hpp"

#include "rpc-stubs/channelgsprpcclient.h"

#include <memory>
#include <atomic>
#include <thread>

namespace xaya
{

/**
 * Instances of this class connect to a channel-game GSP by RPC and feed
 * updates received for a particular channel to a local ChannelManager.
 * This is done through a separate thread that just calls waitforchange
 * and getchannel in a loop.
 */
class ChainToChannelFeeder
{

private:

  /** The RPC connection to the GSP.  */
  ChannelGspRpcClient& rpc;

  /** The ChannelManager that is updated.  */
  SynchronisedChannelManager& manager;

  /** The channel ID in hex.  */
  std::string channelIdHex;

  /** The running thread (if any).  */
  std::unique_ptr<std::thread> loop;

  /** Atomic flag telling the running thread to stop.  */
  std::atomic<bool> stopLoop;

  /**
   * The last block hash to which we updated the channel manager.  This is
   * only accessed from the loop thread (and the constructor).
   */
  uint256 lastBlock;

  /**
   * Queries the GSP for the current state and updates the ChannelManager
   * and lastBlock from the result.
   */
  void UpdateOnce ();

  /**
   * Runs the main loop.  This is the function executed by the loop
   * thread when started.
   */
  void RunLoop ();

public:

  /**
   * Constructs a ChainToChannelFeeder instance based on the given GSP RPC
   * client and ChannelManager to update.  Note that the GSP RPC client will
   * be used from a separate thread and must thus not be used anywhere else
   * at the same time.
   */
  explicit ChainToChannelFeeder (ChannelGspRpcClient& r,
                                 SynchronisedChannelManager& cm);

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
