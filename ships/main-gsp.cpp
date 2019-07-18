// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "logic.hpp"

#include <gamechannel/gsprpc.hpp>
#include <xayagame/defaultmain.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <google/protobuf/stubs/common.h>

#include <cstdlib>
#include <iostream>

namespace
{

DEFINE_string (xaya_rpc_url, "",
               "URL at which Xaya Core's JSON-RPC interface is available");
DEFINE_int32 (game_rpc_port, 0,
              "the port at which the game daemon's JSON-RPC server will be"
              " started (if non-zero)");
DEFINE_bool (game_rpc_listen_locally, true,
             "whether the game daemon's JSON-RPC server should listen locally");

DEFINE_int32 (enable_pruning, -1,
              "if non-negative (including zero), enable pruning of old undo"
              " data and keep as many blocks as specified by the value");

DEFINE_string (datadir, "",
               "base data directory for game data (will be extended by the"
               " game ID and chain)");

} // anonymous namespace

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  gflags::SetUsageMessage ("Run Xayaships game daemon");
  gflags::SetVersionString (PACKAGE_VERSION);
  gflags::ParseCommandLineFlags (&argc, &argv, true);

  if (FLAGS_xaya_rpc_url.empty ())
    {
      std::cerr << "Error: --xaya_rpc_url must be set" << std::endl;
      return EXIT_FAILURE;
    }
  if (FLAGS_datadir.empty ())
    {
      std::cerr << "Error: --datadir must be specified" << std::endl;
      return EXIT_FAILURE;
    }

  xaya::GameDaemonConfiguration config;
  config.XayaRpcUrl = FLAGS_xaya_rpc_url;
  if (FLAGS_game_rpc_port != 0)
    {
      config.GameRpcServer = xaya::RpcServerType::HTTP;
      config.GameRpcPort = FLAGS_game_rpc_port;
      config.GameRpcListenLocally = FLAGS_game_rpc_listen_locally;
    }
  config.EnablePruning = FLAGS_enable_pruning;
  config.DataDirectory = FLAGS_datadir;

  ships::ShipsLogic rules;
  xaya::ChannelGspInstanceFactory instanceFact(rules);
  config.InstanceFactory = &instanceFact;

  const int res = xaya::SQLiteMain (config, "xs", rules);
  google::protobuf::ShutdownProtobufLibrary ();
  return res;
}
