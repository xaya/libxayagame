#include "xayagame/game.hpp"

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <glog/logging.h>

#include <cstdlib>

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);

  CHECK_EQ (argc, 2) << "Usage: rpsd JSON-RPC-URL";

  const std::string jsonRpcUrl(argv[1]);
  jsonrpc::HttpClient httpConnector(jsonRpcUrl);

  xaya::Game game("rps");
  game.ConnectRpcClient (httpConnector);
  CHECK (game.DetectZmqEndpoint ());
  game.Run ();

  return EXIT_SUCCESS;
}
