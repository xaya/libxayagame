#include "logic.hpp"

#include "xayagame/game.hpp"
#include "xayagame/gamerpcserver.hpp"
#include "xayagame/storage.hpp"

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <google/protobuf/stubs/common.h>

#include <glog/logging.h>

#include <cstdlib>

/** Port for the game's RPC server.  */
constexpr unsigned GAME_RPC_PORT = 29050;

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  CHECK_EQ (argc, 2) << "Usage: moverd JSON-RPC-URL";

  const std::string jsonRpcUrl(argv[1]);
  jsonrpc::HttpClient httpConnector(jsonRpcUrl);

  xaya::Game game("mv");
  game.ConnectRpcClient (httpConnector);
  CHECK (game.DetectZmqEndpoint ());

  xaya::MemoryStorage storage;
  game.SetStorage (&storage);

  mover::MoverLogic rules;
  game.SetGameLogic (&rules);

  jsonrpc::HttpServer httpServer(GAME_RPC_PORT);
  xaya::GameRpcServer rpcServer(game, httpServer);

  rpcServer.StartListening ();
  game.Run ();
  rpcServer.StopListening ();

  google::protobuf::ShutdownProtobufLibrary ();
  return EXIT_SUCCESS;
}
