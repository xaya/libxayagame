// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include "uint256.hpp"

#include "rpc-stubs/xayarpcserverstub.h"

#include <json/json.h>
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <sstream>
#include <string>

namespace xaya
{
namespace
{

using testing::_;
using testing::InSequence;
using testing::Return;

constexpr int HTTP_PORT = 32100;

constexpr const char GAME_ID[] = "test-game";

constexpr const char UNITTEST_CHAIN[] = "unittest";

std::string
GetHttpUrl ()
{
  std::ostringstream res;
  res << "http://localhost:" << HTTP_PORT;
  return res.str ();
}

const Json::Value
ParseJson (const std::string& str)
{
  Json::Value val;
  std::istringstream in(str);
  in >> val;
  return val;
}

/**
 * Mock RPC server that takes the place of the Xaya Core daemon in unit tests.
 * Some methods are mocked using GMock, while others (in particular,
 * getblockchaininfo) have an explicit fake implemlentation.
 */
class MockXayaRpcServer : public XayaRpcServerStub
{

private:

  /**
   * Mutex for local state (mainly the settable values for getblockchaininfo).
   * Since the server accesses these from a separate thread, we need to have
   * synchronisation (at least memory barriers).
   */
  std::mutex mut;

  /* Data for the current blockchain tip, as should be returned from
     the getblockchaininfo call.  */
  std::string chain = UNITTEST_CHAIN;
  int height = -1;
  uint256 bestBlock;

public:

  MockXayaRpcServer () = delete;

  explicit MockXayaRpcServer (jsonrpc::AbstractServerConnector& conn)
    : XayaRpcServerStub (conn)
  {
    /* By default, expect no calls to be made.  The calls that we expect
       should explicitly be specified in the individual tests.  */
    EXPECT_CALL (*this, getzmqnotifications ()).Times (0);
    EXPECT_CALL (*this, trackedgames (_, _)).Times (0);
    EXPECT_CALL (*this, getblockhash (_)).Times (0);
    EXPECT_CALL (*this, game_sendupdates (_, _)).Times (0);
  }

  MOCK_METHOD0 (getzmqnotifications, Json::Value ());
  MOCK_METHOD2 (trackedgames, void (const std::string& command,
                                    const std::string& gameid));
  MOCK_METHOD1 (getblockhash, std::string (int height));
  MOCK_METHOD2 (game_sendupdates, Json::Value (const std::string& fromblock,
                                               const std::string& gameid));

  /**
   * Sets the chain value that should be returned for getblockchaininfo.
   * This only needs to be changed from the default if explicit testing of
   * other chain values is desired.
   */
  void
  SetChain (const std::string& c)
  {
    std::lock_guard<std::mutex> lock(mut);
    chain = c;
  }

  /**
   * Sets the data to be returned for the current best block.
   */
  void
  SetBestBlock (const unsigned h, const uint256& hash)
  {
    std::lock_guard<std::mutex> lock(mut);
    height = h;
    bestBlock = hash;
  }

  Json::Value
  getblockchaininfo () override
  {
    CHECK (height >= -1);
    std::lock_guard<std::mutex> lock(mut);

    Json::Value res(Json::objectValue);
    res["chain"] = chain;
    res["blocks"] = height;
    res["bestblockhash"] = bestBlock.ToHex ();

    return res;
  }

};

} // anonymous namespace

class GameTests : public testing::Test
{

protected:

  /** HTTP server connector for the mock server.  */
  jsonrpc::HttpServer httpServer;
  /** Mock for the Xaya daemon RPC server.  */
  MockXayaRpcServer mockXayaServer;
  /** HTTP connection to the mock server for the client.  */
  jsonrpc::HttpClient httpClient;

  /** Some block hash for use in testing.  */
  uint256 blockHash;

  static void
  SetUpTestCase ()
  {
    /* Use JSON-RPC V2 by the RPC client in Game.  It seems that V1 to V1
       does not work with jsonrpccpp for some reason.  */
    Game::rpcClientVersion = jsonrpc::JSONRPC_CLIENT_V2;
  }

  GameTests ()
    : httpServer(HTTP_PORT), mockXayaServer(httpServer),
      httpClient(GetHttpUrl ())
  {
    mockXayaServer.StartListening ();

    CHECK (blockHash.FromHex ("42" + std::string (62, '0')));

    /* The mocked RPC server listens on separate threads and is already set up
       (cannot be started only from within the death test), so we need to run
       those threadsafe.  */
    testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  ~GameTests ()
  {
    mockXayaServer.StopListening ();
  }

  /**
   * Gives tests access to the configured ZMQ endpoint.
   */
  static std::string
  GetZmqEndpoint (const Game& g)
  {
    return g.zmq.addr;
  }

};

namespace
{

/* ************************************************************************** */

using ChainDetectionTests = GameTests;

TEST_F (ChainDetectionTests, ChainDetected)
{
  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, blockHash);
  g.ConnectRpcClient (httpClient);
  EXPECT_EQ (g.GetChain (), UNITTEST_CHAIN);
}

TEST_F (ChainDetectionTests, ReconnectionPossible)
{
  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, blockHash);
  g.ConnectRpcClient (httpClient);
  g.ConnectRpcClient (httpClient);
  EXPECT_EQ (g.GetChain (), UNITTEST_CHAIN);
}

TEST_F (ChainDetectionTests, ReconnectionToWrongChain)
{
  /* For the death test, we need to make sure that we only run the server
     in the forked environment.  If we set up the mock expectations before
     forking, they will be set in both processes, but only fulfillled
     in one of them.  */
  mockXayaServer.StopListening ();

  Game g(GAME_ID);
  EXPECT_DEATH (
    {
      mockXayaServer.StartListening ();
      mockXayaServer.SetBestBlock (0, blockHash);
      g.ConnectRpcClient (httpClient);
      mockXayaServer.SetChain ("otherchain");
      g.ConnectRpcClient (httpClient);
    },
    "Previous RPC connection had chain");
}

/* ************************************************************************** */

using DetectZmqEndpointTests = GameTests;

TEST_F (DetectZmqEndpointTests, Success)
{
  const Json::Value notifications = ParseJson (R"(
    [
      {"address": "foobar"},
      {"type": "sometype", "address": "someaddress"},
      {"type": "pubgameblocks", "address": "address"}
    ]
  )");

  EXPECT_CALL (mockXayaServer, getzmqnotifications ())
      .WillOnce (Return (notifications));

  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, blockHash);
  g.ConnectRpcClient (httpClient);
  ASSERT_TRUE (g.DetectZmqEndpoint ());
  EXPECT_EQ (GetZmqEndpoint (g), "address");
}

TEST_F (DetectZmqEndpointTests, NotSet)
{
  const Json::Value notifications = ParseJson (R"(
    [
      {"address": "foobar"},
      {"type": "sometype", "address": "someaddress"}
    ]
  )");

  EXPECT_CALL (mockXayaServer, getzmqnotifications ())
      .WillOnce (Return (notifications));

  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, blockHash);
  g.ConnectRpcClient (httpClient);
  ASSERT_FALSE (g.DetectZmqEndpoint ());
  EXPECT_EQ (GetZmqEndpoint (g), "");
}

TEST_F (DetectZmqEndpointTests, NoRpcConnection)
{
  Game g(GAME_ID);
  EXPECT_DEATH (
      g.DetectZmqEndpoint (),
      "RPC client is not yet set up");
}

/* ************************************************************************** */

using TrackGameTests = GameTests;

TEST_F (TrackGameTests, CallsMade)
{
  {
    InSequence dummy;
    EXPECT_CALL (mockXayaServer, trackedgames ("add", GAME_ID));
    EXPECT_CALL (mockXayaServer, trackedgames ("remove", GAME_ID));
  }

  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, blockHash);
  g.ConnectRpcClient (httpClient);
  g.TrackGame ();
  g.UntrackGame ();
}

TEST_F (TrackGameTests, NoRpcConnection)
{
  Game g(GAME_ID);
  EXPECT_DEATH (
      g.TrackGame (),
      "RPC client is not yet set up");
  EXPECT_DEATH (
      g.UntrackGame (),
      "RPC client is not yet set up");
}

} // anonymous namespace
} // namespace xaya
