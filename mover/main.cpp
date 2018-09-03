// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "logic.hpp"

#include "xayagame/game.hpp"
#include "xayagame/gamerpcserver.hpp"
#include "xayagame/storage.hpp"

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <google/protobuf/stubs/common.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <iostream>
#include <memory>

DEFINE_string (xaya_rpc_url, "",
               "URL at which Xaya Core's JSON-RPC interface is available");
DEFINE_int32 (game_rpc_port, 0,
              "The port at which the game daemon's JSON-RPC server will be"
              " start (if non-zero)");

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

  const std::string jsonRpcUrl(FLAGS_xaya_rpc_url);
  jsonrpc::HttpClient httpConnector(jsonRpcUrl);

  xaya::Game game("mv");
  game.ConnectRpcClient (httpConnector);
  CHECK (game.DetectZmqEndpoint ());

  xaya::MemoryStorage storage;
  game.SetStorage (&storage);

  mover::MoverLogic rules;
  game.SetGameLogic (&rules);

  std::unique_ptr<jsonrpc::HttpServer> httpServer;
  std::unique_ptr<xaya::GameRpcServer> rpcServer;
  if (FLAGS_game_rpc_port != 0)
    {
      httpServer = std::make_unique<jsonrpc::HttpServer> (FLAGS_game_rpc_port);
      rpcServer = std::make_unique<xaya::GameRpcServer> (game, *httpServer);
    }

  if (rpcServer != nullptr)
    rpcServer->StartListening ();
  game.Run ();
  if (rpcServer != nullptr)
    rpcServer->StopListening ();

  google::protobuf::ShutdownProtobufLibrary ();
  return EXIT_SUCCESS;
}
