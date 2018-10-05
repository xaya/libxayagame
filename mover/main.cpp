// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "logic.hpp"

#include "xayagame/defaultmain.hpp"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <google/protobuf/stubs/common.h>

#include <iostream>

DEFINE_string (xaya_rpc_url, "",
               "URL at which Xaya Core's JSON-RPC interface is available");
DEFINE_int32 (game_rpc_port, 0,
              "The port at which the game daemon's JSON-RPC server will be"
              " start (if non-zero)");

DEFINE_int32 (enable_pruning, -1,
              "If non-negative (including zero), enable pruning of old undo"
              " data and keep as many blocks as specified by the value");

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  gflags::SetUsageMessage ("Run Mover game daemon");
  gflags::SetVersionString (PACKAGE_VERSION);
  gflags::ParseCommandLineFlags (&argc, &argv, true);

  if (FLAGS_xaya_rpc_url.empty ())
    {
      std::cerr << "Error: --xaya_rpc_url must be set" << std::endl;
      return EXIT_FAILURE;
    }

  xaya::GameDaemonConfiguration config;
  config.XayaRpcUrl = FLAGS_xaya_rpc_url;
  config.GameRpcPort = FLAGS_game_rpc_port;
  config.EnablePruning = FLAGS_enable_pruning;

  mover::MoverLogic rules;
  const int res = xaya::DefaultMain (config, "mv", rules);

  google::protobuf::ShutdownProtobufLibrary ();
  return res;
}
