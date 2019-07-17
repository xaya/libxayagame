// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "channel.hpp"

#include <gamechannel/daemon.hpp>
#include <gamechannel/rpcbroadcast.hpp>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>

#include <jsonrpccpp/client/connectors/httpclient.h>

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

  xaya::uint256 channelId;
  if (!channelId.FromHex (FLAGS_channelid))
    {
      std::cerr << "Error: --channelid is invalid" << std::endl;
      return EXIT_FAILURE;
    }

  /* ChannelDaemon manages its own Xaya Core RPC connections, but we need
     a wallet connection also for the ShipsChannel (to construct signed
     winner statements).  */
  jsonrpc::HttpClient xayaWalletClient(FLAGS_xaya_rpc_url);
  XayaWalletRpcClient xayaWallet(xayaWalletClient, jsonrpc::JSONRPC_CLIENT_V1);

  ships::ShipsBoardRules rules;
  ships::ShipsChannel channel(xayaWallet, FLAGS_playername);

  xaya::ChannelDaemon daemon("xs", channelId, FLAGS_playername,
                             rules, channel);
  daemon.ConnectXayaRpc (FLAGS_xaya_rpc_url);
  daemon.ConnectGspRpc (FLAGS_gsp_rpc_url);

  xaya::RpcBroadcast bc(FLAGS_broadcast_rpc_url, daemon.GetChannelManager ());
  daemon.SetOffChainBroadcast (bc);

  daemon.Run ();

  google::protobuf::ShutdownProtobufLibrary ();
  return EXIT_SUCCESS;
}
