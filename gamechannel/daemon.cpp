// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "daemon.hpp"

#include <glog/logging.h>

namespace xaya
{

namespace
{

/**
 * Timeout for the GSP RPC connection.  This must not be too long, as otherwise
 * waitforchange calls may block long and prevent the channel daemon from
 * stopping orderly.
 *
 * The default timeout on the server side is 5s, so with 6s here we ensure
 * that typically the calls return ordinarily; but we put up a safety net
 * in case the server is messed up.
 */
constexpr int GSP_RPC_TIMEOUT_MS = 6'000;

} // anonymous namespace

ChannelDaemon::WalletBasedInstances::WalletBasedInstances (
    ChannelDaemon& d,
    const SignatureVerifier& verifier, SignatureSigner& signer,
    TransactionSender& txSender)
  : sender(d.gameId, d.channelId, d.playerName, txSender, d.channel),
    cm(d.rules, d.channel, verifier, signer, d.channelId, d.playerName)
{
  cm.SetMoveSender (sender);
}

ChannelDaemon::WalletBasedInstances::~WalletBasedInstances ()
{
  cm.StopUpdates ();
}

ChannelDaemon::GspFeederInstances::GspFeederInstances (ChannelDaemon& d,
                                                       const std::string& rpc)
  : gspClient(rpc), gspRpc(gspClient),
    feeder(gspRpc, d.walletBased->cm)
{
  gspClient.SetTimeout (GSP_RPC_TIMEOUT_MS);
}

void
ChannelDaemon::ConnectWallet (const SignatureVerifier& v, SignatureSigner& s,
                              TransactionSender& tx)
{
  CHECK (walletBased == nullptr);
  walletBased = std::make_unique<WalletBasedInstances> (*this, v, s, tx);
}

void
ChannelDaemon::ConnectGspRpc (const std::string& url)
{
  CHECK (walletBased != nullptr);
  CHECK (feeder == nullptr);
  feeder = std::make_unique<GspFeederInstances> (*this, url);
}

ChannelManager&
ChannelDaemon::GetChannelManager ()
{
  CHECK (walletBased != nullptr);
  return walletBased->cm;
}

void
ChannelDaemon::SetOffChainBroadcast (OffChainBroadcast& b)
{
  CHECK (walletBased != nullptr);
  CHECK (offChain == nullptr);
  offChain = &b;
  walletBased->cm.SetOffChainBroadcast (*offChain);
}

void
ChannelDaemon::Start ()
{
  CHECK (walletBased != nullptr);
  CHECK (feeder != nullptr);
  CHECK (offChain != nullptr);
  CHECK (!startedOnce);
  startedOnce = true;

  feeder->feeder.Start ();
  offChain->Start ();
}

void
ChannelDaemon::Stop ()
{
  CHECK (walletBased != nullptr);
  CHECK (feeder != nullptr);
  CHECK (offChain != nullptr);
  CHECK (startedOnce);

  feeder->feeder.Stop ();
  offChain->Stop ();
  walletBased->cm.StopUpdates ();
}

void
ChannelDaemon::Run ()
{
  internal::MainLoop::Functor startAction = [this] () { Start (); };
  internal::MainLoop::Functor stopAction = [this] () { Stop (); };

  mainLoop.Run (startAction, stopAction);
}

} // namespace xaya
