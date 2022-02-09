// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "channel.hpp"
#include "channelrpc.hpp"

#include <gamechannel/daemon.hpp>
#include <gamechannel/ethsignatures.hpp>
#include <gamechannel/rpcbroadcast.hpp>
#include <gamechannel/rpcwallet.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>

#include <eth-utils/ecdsa.hpp>

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <google/protobuf/stubs/common.h>

#include <cstdlib>
#include <iostream>

namespace
{

DEFINE_string (xaya_rpc_url, "",
               "URL at which Xaya Core's JSON-RPC interface is available"
               " including a wallet");
DEFINE_bool (xaya_rpc_legacy_protocol, true,
             "whether to use JSON-RPC 1.0 instead of 2.0 for the Xaya RPC;"
             " this is needed for Xaya Core, whereas other servers"
             " like Electrum-CHI should use JSON-RPC 2.0");
DEFINE_string (gsp_rpc_url, "",
               "URL at which the shipsd JSON-RPC interface is available");
DEFINE_string (broadcast_rpc_url, "",
               "URL at which the broadcast server's JSON-RPC interface"
               " is available");

DEFINE_int32 (rpc_port, 0,
              "the port at which the channel daemon's JSON-RPC server will be"
              " started (if non-zero)");
DEFINE_bool (rpc_listen_locally, true,
             "whether the JSON-RPC server should listen locally");

DEFINE_string (playername, "",
               "the Xaya name of the player for this channel (without p/)");
DEFINE_string (privkey, "",
               "the Ethereum private key used for signing on the channel");
DEFINE_string (channelid, "", "ID of the channel to manage as hex string");

} // anonymous namespace

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  gflags::SetUsageMessage ("Run Xayaships channel daemon");
  gflags::SetVersionString (PACKAGE_VERSION);
  gflags::ParseCommandLineFlags (&argc, &argv, true);

  if (FLAGS_xaya_rpc_url.empty ())
    {
      std::cerr << "Error: --xaya_rpc_url must be set" << std::endl;
      return EXIT_FAILURE;
    }
  if (FLAGS_gsp_rpc_url.empty ())
    {
      std::cerr << "Error: --gsp_rpc_url must be set" << std::endl;
      return EXIT_FAILURE;
    }
  if (FLAGS_broadcast_rpc_url.empty ())
    {
      std::cerr << "Error: --broadcast_rpc_url must be set" << std::endl;
      return EXIT_FAILURE;
    }

  if (FLAGS_playername.empty ())
    {
      std::cerr << "Error: --playername must be set" << std::endl;
      return EXIT_FAILURE;
    }
  if (FLAGS_privkey.empty ())
    {
      std::cerr << "Error: --privkey must be set" << std::endl;
      return EXIT_FAILURE;
    }

  xaya::uint256 channelId;
  if (!channelId.FromHex (FLAGS_channelid))
    {
      std::cerr << "Error: --channelid is invalid" << std::endl;
      return EXIT_FAILURE;
    }

  // FIXME: Clean up completely once the move sender is migrated as well.
  const auto rpcVersion = (FLAGS_xaya_rpc_legacy_protocol
                              ? jsonrpc::JSONRPC_CLIENT_V1
                              : jsonrpc::JSONRPC_CLIENT_V2);
  jsonrpc::HttpClient xayaClient(FLAGS_xaya_rpc_url);
  XayaRpcClient xayaRpc(xayaClient, rpcVersion);
  XayaWalletRpcClient xayaWallet(xayaClient, rpcVersion);

  const ethutils::ECDSA ecdsaCtx;
  const xaya::EthSignatureVerifier verifier(ecdsaCtx);
  xaya::EthSignatureSigner signer(ecdsaCtx, FLAGS_privkey);
  xaya::RpcTransactionSender sender(xayaRpc, xayaWallet);

  ships::ShipsBoardRules rules;
  ships::ShipsChannel channel(FLAGS_playername);

  xaya::ChannelDaemon daemon("xs", channelId, FLAGS_playername, rules, channel);
  daemon.ConnectWallet (verifier, signer, sender);
  daemon.ConnectGspRpc (FLAGS_gsp_rpc_url);

  xaya::RpcBroadcast bc(FLAGS_broadcast_rpc_url, daemon.GetChannelManager ());
  daemon.SetOffChainBroadcast (bc);

  std::unique_ptr<jsonrpc::AbstractServerConnector> serverConnector;
  if (FLAGS_rpc_port != 0)
    {
      auto srv = std::make_unique<jsonrpc::HttpServer> (FLAGS_rpc_port);
      if (FLAGS_rpc_listen_locally)
        srv->BindLocalhost ();
      serverConnector = std::move (srv);
      LOG (INFO) << "Starting JSON-RPC HTTP server at port " << FLAGS_rpc_port;
    }

  std::unique_ptr<ships::ShipsChannelRpcServer> rpcServer;
  if (serverConnector != nullptr)
    {
      rpcServer = std::make_unique<ships::ShipsChannelRpcServer> (
                      channel, daemon, *serverConnector);
      rpcServer->StartListening ();
    }
  else
    LOG (WARNING) << "Channel daemon has no JSON-RPC interface";

  daemon.Run ();

  if (rpcServer != nullptr)
    rpcServer->StopListening ();

  google::protobuf::ShutdownProtobufLibrary ();
  return EXIT_SUCCESS;
}
