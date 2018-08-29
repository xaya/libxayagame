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

#include <cstdio>
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

constexpr const char NO_REQ_TOKEN[] = "";

constexpr bool SEQ_MISMATCH = true;
constexpr bool NO_SEQ_MISMATCH = false;

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
 * Returns a uint256 based on the given number, to be used as block hashes
 * in tests.
 */
uint256
BlockHash (const unsigned num)
{
  std::string hex = "ab" + std::string (62, '0');

  CHECK (num < 0x100);
  std::sprintf (&hex[2], "%02x", num);
  CHECK (hex[4] == 0);
  hex[4] = '0';

  uint256 res;
  CHECK (res.FromHex (hex));
  return res;
}

/* ************************************************************************** */

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

/* ************************************************************************** */

constexpr unsigned GAME_GENESIS_HEIGHT = 10;
constexpr const char GAME_GENESIS_HASH[]
    = "0000000000000000000000000000000000000000000000000000000000000010";

/**
 * Very simple game rules that are used in the unit tests.  This just allows
 * one-letter names to set a one-character "value" for themselves in the game
 * state.
 *
 * Moves look like this:
 *
 *    {
 *      "a": "=",
 *      "x": "0"
 *    }
 *
 * The game state is a string that just holds the set names and their values
 * in ascending order.  For the situation of the move above, it would be:
 *
 *    a=x0
 *
 * Undo data is a string with all the names that were first created (rather
 * than changed) in this block, so they can be fully removed from the state:
 *
 *    ax
 */
class TestGame : public GameLogic
{

public:

  void
  GetInitialState (const std::string& chain,
                   unsigned& height, std::string& hashHex,
                   GameStateData& state) override
  {
    CHECK_EQ (chain, UNITTEST_CHAIN);
    height = GAME_GENESIS_HEIGHT;
    hashHex = GAME_GENESIS_HASH;
    state = "";
  }

  static uint256
  GenesisBlockHash ()
  {
    uint256 res;
    CHECK (res.FromHex (GAME_GENESIS_HASH));
    return res;
  }

};

/* ************************************************************************** */

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

  /** In-memory storage that can be used in tests.  */
  MemoryStorage storage;
  /** Game rules for the test game.  */
  TestGame rules;

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

    /* The mocked RPC server listens on separate threads and is already set up
       (cannot be started only from within the death test), so we need to run
       those threadsafe.  */
    testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  ~GameTests ()
  {
    mockXayaServer.StopListening ();
  }

  /* Make some private internals of Game accessible to tests.  */

  using State = Game::State;

  static std::string
  GetZmqEndpoint (const Game& g)
  {
    return g.zmq.addr;
  }

  static State
  GetState (const Game& g)
  {
    return g.state;
  }

  static void
  ReinitialiseState (Game& g)
  {
    std::lock_guard<std::mutex> lock(g.mut);
    g.ReinitialiseState ();
  }

  static void
  CallBlockAttach (Game& g, const std::string& reqToken,
                   const uint256& parentHash, const uint256& childHash,
                   const Json::Value& moves, const bool seqMismatch)
  {
    Json::Value data(Json::objectValue);
    if (!reqToken.empty ())
      data["reqtoken"] = reqToken;
    data["parent"] = parentHash.ToHex ();
    data["child"] = childHash.ToHex ();
    data["moves"] = moves;

    g.BlockAttach (GAME_ID, data, seqMismatch);
  }

};

namespace
{

/* ************************************************************************** */

using ChainDetectionTests = GameTests;

TEST_F (ChainDetectionTests, ChainDetected)
{
  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, BlockHash (0));
  g.ConnectRpcClient (httpClient);
  EXPECT_EQ (g.GetChain (), UNITTEST_CHAIN);
}

TEST_F (ChainDetectionTests, ReconnectionPossible)
{
  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, BlockHash (0));
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
      mockXayaServer.SetBestBlock (0, BlockHash (0));
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
  mockXayaServer.SetBestBlock (0, BlockHash (0));
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
  mockXayaServer.SetBestBlock (0, BlockHash (0));
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
  mockXayaServer.SetBestBlock (0, BlockHash (0));
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

/* ************************************************************************** */

class InitialStateTests : public GameTests
{

protected:

  Game g;

  InitialStateTests()
    : g(GAME_ID)
  {
    EXPECT_CALL (mockXayaServer, getblockhash (GAME_GENESIS_HEIGHT))
        .WillRepeatedly (Return (GAME_GENESIS_HASH));

    mockXayaServer.SetBestBlock (0, BlockHash (0));
    g.ConnectRpcClient (httpClient);

    g.SetStorage (&storage);
    g.SetGameLogic (&rules);
  }

  void
  ExpectInitialStateInStorage () const
  {
    uint256 hash;
    EXPECT_TRUE (storage.GetCurrentBlockHash (hash));
    EXPECT_EQ (hash, TestGame::GenesisBlockHash ());
    EXPECT_EQ (storage.GetCurrentGameState (), "");
  }

};

TEST_F (InitialStateTests, BeforeGenesis)
{
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  uint256 hash;
  EXPECT_FALSE (storage.GetCurrentBlockHash (hash));
}

TEST_F (InitialStateTests, AfterGenesis)
{
  mockXayaServer.SetBestBlock (20, BlockHash (20));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::OUT_OF_SYNC);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, WaitingForGenesis)
{
  const Json::Value emptyMoves(Json::objectValue);

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (9, BlockHash (9));
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (8), BlockHash (9),
                   emptyMoves, NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  CallBlockAttach (g, NO_REQ_TOKEN,
                   BlockHash (9), TestGame::GenesisBlockHash (),
                   emptyMoves, NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::OUT_OF_SYNC);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, MissedNotification)
{
  const Json::Value emptyMoves(Json::objectValue);

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (9, BlockHash (9));
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (8), BlockHash (9),
                   emptyMoves, NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (20, TestGame::GenesisBlockHash ());
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (19), BlockHash (20),
                   emptyMoves, SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::OUT_OF_SYNC);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, MismatchingGenesisHash)
{
  EXPECT_CALL (mockXayaServer, getblockhash (GAME_GENESIS_HEIGHT))
      .WillRepeatedly (Return (std::string (64, '0')));

  mockXayaServer.SetBestBlock (20, BlockHash (20));
  EXPECT_DEATH (
      ReinitialiseState (g),
      "genesis block hash and height do not match");
}

} // anonymous namespace
} // namespace xaya
