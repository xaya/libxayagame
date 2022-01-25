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
 * Then call ConnectWallet, then ConnectGspRpc, then SetOffChainBroadcast
 * and then Start/Stop or Run.
 */
class ChannelDaemon
{

private:

  /**
   * The signature signers/verifiers and transaction sender, as well
   * as the ChannelManager instance using them.
   *
   * These are the things that can get constructed when the underlying
   * "wallet" is connected.
   */
  struct WalletBasedInstances
  {

    /** The MoveSender instance we use.  */
    MoveSender sender;

    /** The ChannelManager instance used.  */
    ChannelManager realCm;
    /** The lock wrapper around the channel manager.  */
    SynchronisedChannelManager cm;

    WalletBasedInstances () = delete;
    WalletBasedInstances (const WalletBasedInstances&) = delete;
    void operator= (const WalletBasedInstances&) = delete;

    explicit WalletBasedInstances (
        ChannelDaemon& daemon,
        const SignatureVerifier& v, SignatureSigner& s,
        TransactionSender& tx);
    ~WalletBasedInstances ();

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

  /** The board rules for this game.  */
  const BoardRules& rules;

  /** The OpenChannel instance for this game.  */
  OpenChannel& channel;

  /** MainLoop for this daemon.  */
  internal::MainLoop mainLoop;

  /**
   * Instances of components based on the wallet.  This is set by ConnectWallet.
   */
  std::unique_ptr<WalletBasedInstances> walletBased;

  /** Instances of the GSP RPC connection and dependencies.  */
  std::unique_ptr<GspFeederInstances> feeder;

  /** The broadcast instance we use.  */
  ReceivingOffChainBroadcast* offChain = nullptr;

  /**
   * Set to true when the daemon is started for the first time.  Since we
   * do not support restarting again (mainly because ChannelManager::StopUpdates
   * is irreversible), this is used to verify that.
   */
  bool startedOnce = false;

public:

  explicit ChannelDaemon (const std::string& gid,
                          const uint256& id, const std::string& nm,
                          const BoardRules& r, OpenChannel& oc)
    : gameId(gid), channelId(id), playerName(nm),
      rules(r), channel(oc)
  {}

  ChannelDaemon () = delete;
  ChannelDaemon (const ChannelDaemon&) = delete;
  void operator= (const ChannelDaemon&) = delete;

  /**
   * Connects the blockchain "wallet" (defining the signature scheme and
   * the connector used for triggering automatic on-chain transactions).
   * This can e.g. be based on a Xaya Core RPC.
   */
  void ConnectWallet (const SignatureVerifier& v, SignatureSigner& s,
                      TransactionSender& tx);

  /**
   * Connects the GSP RPC URL and initialises the dependencies on that.
   * This must be called after ConnectWallet and before starting.
   */
  void ConnectGspRpc (const std::string& url);

  /**
   * Returns a reference to the underlying ChannelManager, which can be used
   * for constructing the OffChainBroadcast and/or RPC server externally.
   */
  SynchronisedChannelManager& GetChannelManager ();

  /**
   * Sets the off-chain broadcast instance.  This must be called before
   * starting.  The instance must be constructed and managed externally
   * (based on the desired broadcast system).
   */
  void SetOffChainBroadcast (ReceivingOffChainBroadcast& b);

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
