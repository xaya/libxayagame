// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_DAEMON_HPP
#define GAMECHANNEL_DAEMON_HPP

#include "boardrules.hpp"
#include "broadcast.hpp"
#include "chaintochannel.hpp"
#include "channelmanager.hpp"
#include "movesender.hpp"
#include "openchannel.hpp"
#include "signatures.hpp"

#include "rpc-stubs/channelgsprpcclient.h"

#include <xayagame/mainloop.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayautil/uint256.hpp>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <string>

namespace xaya
{

/**
 * The "main class" of a channel daemon.  This puts together a ChannelManager
 * instance with the various external interfaces (for feeding updates into
 * it and sending updates to the world).  It also manages a main loop that
 * can be run to block until stopped or signalled.
 *
 * Note that initialisation of the various components must be done in the
 * right order.  To correctly use this class, first call the constructor.
 * Then call ConnectXayaRpc, then ConnectGspRpc, then SetOffChainBroadcast
 * and then Start/Stop or Run.
 */
class ChannelDaemon
{

private:

  /**
   * Xaya Core RPC clients and a ChannelManager instance using them.
   * These are the things that can get constructed when the Xaya Core RPC
   * is connected.
   */
  struct XayaBasedInstances
  {

    /** The HTTP client for Xaya Core RPC.  */
    jsonrpc::HttpClient xayaClient;
    /** The RPC client for Xaya Core (not wallet).  */
    XayaRpcClient xayaRpc;
    /** The RPC client for the Xaya wallet.  */
    XayaWalletRpcClient xayaWallet;

    /** Signature verifier (based on the RPC).  */
    RpcSignatureVerifier verifier;
    /** Signature signer based on RPC.  */
    RpcSignatureSigner signer;

    /** The ChannelManager instance.  */
    ChannelManager cm;

    /** The MoveSender instance.  */
    MoveSender sender;

    XayaBasedInstances () = delete;
    XayaBasedInstances (const XayaBasedInstances&) = delete;
    void operator= (const XayaBasedInstances&) = delete;

    explicit XayaBasedInstances (ChannelDaemon& daemon, const std::string& addr,
                                 const std::string& rpc,
                                 jsonrpc::clientVersion_t rpcVersion);
    ~XayaBasedInstances ();

  };

  /**
   * ChainToChannelFeeder instance and its GSP RPC connection.
   */
  struct GspFeederInstances
  {

    /** The HTTP client for the GSP RPC.  */
    jsonrpc::HttpClient gspClient;
    /** The RPC client for the GSP.  */
    ChannelGspRpcClient gspRpc;

    /** The ChainToChannelFeeder instance.  */
    ChainToChannelFeeder feeder;

    GspFeederInstances () = delete;
    GspFeederInstances (const GspFeederInstances&) = delete;
    void operator= (const GspFeederInstances&) = delete;

    explicit GspFeederInstances (ChannelDaemon& daemon, const std::string& rpc);

  };

  /** The game ID.  */
  const std::string gameId;

  /** The channel ID.  */
  const uint256 channelId;

  /** The player's name (without p/ prefix).  */
  const std::string playerName;
  /** The player's signing address.  */
  const std::string address;

  /** The board rules for this game.  */
  const BoardRules& rules;

  /** The OpenChannel instance for this game.  */
  OpenChannel& channel;

  /** MainLoop for this daemon.  */
  internal::MainLoop mainLoop;

  /**
   * Instances of components based on the Xaya Core RPC connection.  This is
   * set by ConnectXayaRpc.
   */
  std::unique_ptr<XayaBasedInstances> xayaBased;

  /** Instances of the GSP RPC connection and dependencies.  */
  std::unique_ptr<GspFeederInstances> feeder;

  /** The broadcast instance we use.  */
  OffChainBroadcast* offChain = nullptr;

  /**
   * Set to true when the daemon is started for the first time.  Since we
   * do not support restarting again (mainly because ChannelManager::StopUpdates
   * is irreversible), this is used to verify that.
   */
  bool startedOnce = false;

public:

  explicit ChannelDaemon (const std::string& gid,
                          const uint256& id,
                          const std::string& nm, const std::string& addr,
                          const BoardRules& r, OpenChannel& oc)
    : gameId(gid), channelId(id), playerName(nm), address(addr),
      rules(r), channel(oc)
  {}

  ChannelDaemon () = delete;
  ChannelDaemon (const ChannelDaemon&) = delete;
  void operator= (const ChannelDaemon&) = delete;

  /**
   * Sets the RPC URL to use for Xaya Core.  This must be called exactly
   * once before the ChannelDaemon becomes available.  By default, the RPC
   * client uses JSON-RPC 1.0 to be compatible with Xaya Core.  For using
   * JSON-RPC 2.0 (e.g. to use it with an Electrum-based light client),
   * set legacy to false.
   */
  void ConnectXayaRpc (const std::string& url, bool legacy = true);

  /**
   * Connects the GSP RPC URL and initialises the dependencies on that.
   * This must be called after ConnectXayaRpc and before starting.
   */
  void ConnectGspRpc (const std::string& url);

  /**
   * Returns a reference to the underlying ChannelManager, which can be used
   * for constructing the OffChainBroadcast and/or RPC server externally.
   */
  ChannelManager& GetChannelManager ();

  /**
   * Sets the off-chain broadcast instance.  This must be called before
   * starting.  The instance must be constructed and managed externally
   * (based on the desired broadcast system).
   */
  void SetOffChainBroadcast (OffChainBroadcast& b);

  /**
   * Requests the mainloop to stop, e.g. from an RPC.
   */
  void
  RequestStop ()
  {
    mainLoop.Stop ();
  }

  /**
   * Starts all components after they are initialised.  This must only
   * be called once.
   */
  void Start ();

  /**
   * Stops the running components.
   */
  void Stop ();

  /**
   * Runs a main loop.  This starts the daemon as with Start(), blocks until
   * requested to stop with RequestStop() or signalled, and then stops
   * everything as with Stop().
   */
  void Run ();

};

} // namespace xaya

#endif // GAMECHANNEL_DAEMON_HPP
