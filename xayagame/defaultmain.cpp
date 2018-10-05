// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "defaultmain.hpp"

#include "game.hpp"
#include "gamerpcserver.hpp"
#include "storage.hpp"

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <glog/logging.h>

#include <cstdlib>
#include <memory>

namespace xaya
{

int
DefaultMain (const GameDaemonConfiguration& config,
             const std::string& gameId,
             GameLogic& rules)
{
  CHECK (!config.XayaRpcUrl.empty ()) << "XayaRpcUrl must be configured";
  const std::string jsonRpcUrl(config.XayaRpcUrl);
  jsonrpc::HttpClient httpConnector(jsonRpcUrl);

  xaya::Game game(gameId);
  game.ConnectRpcClient (httpConnector);
  CHECK (game.DetectZmqEndpoint ());

  xaya::MemoryStorage storage;
  game.SetStorage (&storage);

  game.SetGameLogic (&rules);

  if (config.EnablePruning >= 0)
    game.EnablePruning (config.EnablePruning);

  std::unique_ptr<jsonrpc::HttpServer> httpServer;
  std::unique_ptr<xaya::GameRpcServer> rpcServer;
  if (config.GameRpcPort != 0)
    {
      httpServer = std::make_unique<jsonrpc::HttpServer> (config.GameRpcPort);
      rpcServer = std::make_unique<xaya::GameRpcServer> (game, *httpServer);
    }

  if (rpcServer != nullptr)
    rpcServer->StartListening ();
  game.Run ();
  if (rpcServer != nullptr)
    rpcServer->StopListening ();

  return EXIT_SUCCESS;
}

} // namespace xaya
