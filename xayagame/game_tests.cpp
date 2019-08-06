// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include "gamelogic.hpp"

#include "testutils.hpp"

#include "rpc-stubs/xayarpcserverstub.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <cstdio>
#include <map>
#include <sstream>
#include <string>

namespace xaya
{
namespace
{

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Return;

constexpr const char GAME_ID[] = "test-game";

constexpr const char NO_REQ_TOKEN[] = "";

constexpr bool SEQ_MISMATCH = true;
constexpr bool NO_SEQ_MISMATCH = false;

/* ************************************************************************** */

/**
 * Mock RPC server that takes the place of the Xaya Core daemon in unit tests.
 * Some methods are mocked using GMock, while others (in particular,
 * getblockchaininfo) have an explicit fake implemlentation.
 */
class MockXayaRpcServerWithState : public MockXayaRpcServer
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
  Chain chain = Chain::MAIN;
  int height = -1;
  uint256 bestBlock;

public:

  using MockXayaRpcServer::MockXayaRpcServer;

  /**
   * Sets the chain value that should be returned for getblockchaininfo.
   * This only needs to be changed from the default if explicit testing of
   * other chain values is desired.
   */
  void
  SetChain (const Chain c)
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
    res["chain"] = ChainToString (chain);
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
 * Moves are just single-character strings that define the new value
 * for a name.  Two moves in a block could look like this:
 *
 *    {
 *      ...
 *      "moves":
 *        [
 *          {
 *            ...
 *            "name": "a",
 *            "move": "="
 *          },
 *          {
 *            ...
 *            "name": "x",
 *            "move": "0"
 *          }
 *        ]
 *    }
 *
 * The game state is a string that just holds the set names and their values
 * in ascending order.  For the situation of the block above, it would be:
 *
 *    a=x0
 *
 * Undo data is a string with all the names that were updated in this block,
 * together with their previous values.  The format is the same as in the game
 * state.  Names that were created (rather than updated) have the previous
 * value set to ".", which has a special meaning here.  So for instance, if
 * "a" would have existed already with value "-" and "x" would have been
 * created by the example move above, then the undo data would be:
 *
 *    a-x.
 */
class TestGame : public GameLogic
{

private:

  /** A map holding name/value pairs.  */
  using Map = std::map<std::string, std::string>;

  /**
   * Parses a string of the game state / undo format into a map holding
   * the name/value pairs.
   */
  static Map
  DecodeMap (const std::string& str)
  {
    CHECK_EQ (str.size () % 2, 0);
    Map result;
    for (size_t i = 0; i < str.size (); i += 2)
      result.emplace (str.substr (i, 1), str.substr (i + 1, 1));
    return result;
  }

  /**
   * Encodes a name/value map into a string for game state / undo data.
   */
  static std::string
  EncodeMap (const Map& m)
  {
    std::ostringstream res;
    for (const auto& e : m)
      {
        CHECK_EQ (e.first.size (), 1);
        CHECK_EQ (e.second.size (), 1);
        res << e.first << e.second;
      }
    return res.str ();
  }

protected:

  GameStateData
  GetInitialStateInternal (unsigned& height, std::string& hashHex) override
  {
    CHECK (GetContext ().GetChain () == Chain::MAIN);
    CHECK_EQ (GetContext ().GetGameId (), GAME_ID);

    height = GAME_GENESIS_HEIGHT;
    hashHex = GAME_GENESIS_HASH;
    return EncodeMap (Map ());
  }

  GameStateData
  ProcessForwardInternal (const GameStateData& oldState,
                          const Json::Value& blockData,
                          UndoData& undoData) override
  {
    CHECK (GetContext ().GetChain () == Chain::MAIN);
    CHECK_EQ (GetContext ().GetGameId (), GAME_ID);

    Map state = DecodeMap (oldState);
    Map undo;

    for (const auto& m : blockData["moves"])
      {
        const std::string name = m["name"].asString ();
        const std::string value = m["move"].asString ();
        CHECK_NE (value, ".");

        const auto mi = state.find (name);
        if (mi == state.end ())
          {
            undo.emplace (name, ".");
            state.emplace (name, value);
          }
        else
          {
            undo.emplace (name, mi->second);
            mi->second = value;
          }
      }

    undoData = EncodeMap (undo);
    return EncodeMap (state);
  }

  GameStateData
  ProcessBackwardsInternal (const GameStateData& newState,
                            const Json::Value& blockData,
                            const UndoData& undoData) override
  {
    CHECK (GetContext ().GetChain () == Chain::MAIN);
    CHECK_EQ (GetContext ().GetGameId (), GAME_ID);

    Map state = DecodeMap (newState);
    const Map undo = DecodeMap (undoData);

    for (const auto& e : undo)
      {
        if (e.second == ".")
          state.erase (e.first);
        else
          state[e.first] = e.second;
      }

    return EncodeMap (state);
  }

public:

  Json::Value
  GameStateToJson (const GameStateData& state) override
  {
    Json::Value res(Json::objectValue);
    res["state"] = state;
    return res;
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

class GameTests : public GameTestWithBlockchain
{

protected:

  /** HTTP server connector for the mock server.  */
  jsonrpc::HttpServer httpServer;
  /** Mock for the Xaya daemon RPC server.  */
  MockXayaRpcServerWithState mockXayaServer;
  /** HTTP connection to the mock server for the client.  */
  jsonrpc::HttpClient httpClient;

  /** In-memory storage that can be used in tests.  */
  MemoryStorage storage;
  /** Game rules for the test game.  */
  TestGame rules;

  GameTests ()
    : GameTestWithBlockchain(GAME_ID),
      httpServer(MockXayaRpcServer::HTTP_PORT), mockXayaServer(httpServer),
      httpClient(MockXayaRpcServer::HTTP_URL)
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

  void
  CallBlockDetach (Game& g, const std::string& reqToken,
                   const uint256& parentHash, const uint256& blockHash,
                   const unsigned height,
                   const bool seqMismatch) const
  {
    /* For our example test game, the moves are not used for rolling backwards.
       Thus just set an empty string.  */
    GameTestFixture::CallBlockDetach (g, reqToken,
                                      parentHash, blockHash, height,
                                      "", seqMismatch);
  }

};

/* ************************************************************************** */

using XayaVersionTests = GameTests;

TEST_F (XayaVersionTests, Works)
{
  Json::Value networkInfo(Json::objectValue);
  networkInfo["version"] = 1020300;
  EXPECT_CALL (mockXayaServer, getnetworkinfo ())
      .WillOnce (Return (networkInfo));

  Game g(GAME_ID);
  g.ConnectRpcClient (httpClient);
  EXPECT_EQ (g.GetXayaVersion (), 1020300);
}

/* ************************************************************************** */

using ChainDetectionTests = GameTests;

TEST_F (ChainDetectionTests, ChainDetected)
{
  Game g(GAME_ID);
  mockXayaServer.SetBestBlock (0, BlockHash (0));
  g.ConnectRpcClient (httpClient);
  EXPECT_TRUE (g.GetChain () == Chain::MAIN);
}

TEST_F (ChainDetectionTests, Reconnection)
{
  /* For the death test, we need to make sure that we only run the server
     in the forked environment.  If we set up the mock expectations before
     forking, they will be set in both processes, but only fulfilled
     in one of them.  */
  mockXayaServer.StopListening ();

  Game g(GAME_ID);
  EXPECT_DEATH (
    {
      mockXayaServer.StartListening ();
      mockXayaServer.SetBestBlock (0, BlockHash (0));
      g.ConnectRpcClient (httpClient);
      g.ConnectRpcClient (httpClient);
    },
    "RPC client is already connected");
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
  TrackGame (g);
  UntrackGame (g);
}

TEST_F (TrackGameTests, NoRpcConnection)
{
  Game g(GAME_ID);
  EXPECT_DEATH (
      TrackGame (g),
      "RPC client is not yet set up");
  EXPECT_DEATH (
      UntrackGame (g),
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

    g.SetStorage (storage);
    g.SetGameLogic (rules);
  }

  void
  ExpectInitialStateInStorage () const
  {
    uint256 hash;
    EXPECT_TRUE (storage.GetCurrentBlockHash (hash));
    EXPECT_EQ (hash, TestGame::GenesisBlockHash ());
    EXPECT_EQ (storage.GetCurrentGameState (), "");
  }

  /**
   * Converts a string in the game-state format to a series of moves as they
   * would appear in the block notification.
   */
  static Json::Value
  Moves (const std::string& str)
  {
    Json::Value moves(Json::arrayValue);

    CHECK_EQ (str.size () % 2, 0);
    for (size_t i = 0; i < str.size (); i += 2)
      {
        Json::Value obj(Json::objectValue);
        obj["name"] = str.substr (i, 1);
        obj["move"] = str.substr (i + 1, 1);
        moves.append (obj);
      }

    return moves;
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
  Json::Value upd(Json::objectValue);
  upd["toblock"] = BlockHash (20).ToHex ();
  upd["reqtoken"] = "reqtoken";
  EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (upd));

  mockXayaServer.SetBestBlock (20, BlockHash (20));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, WaitingForGenesis)
{
  const Json::Value emptyMoves(Json::objectValue);

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (9, BlockHash (9));
  SetStartingBlock (BlockHash (8));
  AttachBlock (g, BlockHash (9), emptyMoves);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  AttachBlock (g, TestGame::GenesisBlockHash (), emptyMoves);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, MissedNotification)
{
  const Json::Value emptyMoves(Json::objectValue);

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (9, BlockHash (9));
  SetStartingBlock (BlockHash (8));
  AttachBlock (g, BlockHash (9), emptyMoves);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer.SetBestBlock (20, TestGame::GenesisBlockHash ());
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (19), BlockHash (20), 20,
                   emptyMoves, SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
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

/* ************************************************************************** */

using GetCurrentJsonStateTests = InitialStateTests;

TEST_F (GetCurrentJsonStateTests, NoStateYet)
{
  const Json::Value state = g.GetCurrentJsonState ();
  EXPECT_EQ (state["gameid"], GAME_ID);
  EXPECT_EQ (state["chain"], "main");
  EXPECT_EQ (state["state"], "unknown");
  EXPECT_FALSE (state.isMember ("blockhash"));
  EXPECT_FALSE (state.isMember ("height"));
  EXPECT_FALSE (state.isMember ("gamestate"));
}

TEST_F (GetCurrentJsonStateTests, InitialState)
{
  mockXayaServer.SetBestBlock (GAME_GENESIS_HEIGHT,
                               TestGame::GenesisBlockHash ());
  ReinitialiseState (g);

  const Json::Value state = g.GetCurrentJsonState ();
  EXPECT_EQ (state["gameid"], GAME_ID);
  EXPECT_EQ (state["chain"], "main");
  EXPECT_EQ (state["state"], "up-to-date");
  EXPECT_EQ (state["blockhash"], GAME_GENESIS_HASH);
  EXPECT_EQ (state["height"].asInt (), GAME_GENESIS_HEIGHT);
  EXPECT_EQ (state["gamestate"]["state"], "");
}

TEST_F (GetCurrentJsonStateTests, WhenUpToDate)
{
  mockXayaServer.SetBestBlock (GAME_GENESIS_HEIGHT,
                               TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  SetStartingBlock (TestGame::GenesisBlockHash ());
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));

  const Json::Value state = g.GetCurrentJsonState ();
  EXPECT_EQ (state["gameid"], GAME_ID);
  EXPECT_EQ (state["chain"], "main");
  EXPECT_EQ (state["state"], "up-to-date");
  EXPECT_EQ (state["blockhash"], BlockHash (11).ToHex ());
  EXPECT_EQ (state["gamestate"]["state"], "a0b1");

  /* The expected cached height is two, since we only attached two blocks
     (even though we use BlockHash(11) for it).  */
  EXPECT_EQ (state["height"].asInt (), 2);
}

TEST_F (GetCurrentJsonStateTests, HeightResolvedViaRpc)
{
  Json::Value blockHeaderData(Json::objectValue);
  blockHeaderData["height"] = 42;
  EXPECT_CALL (mockXayaServer, getblockheader (GAME_GENESIS_HASH))
      .WillOnce (Return (blockHeaderData));

  mockXayaServer.SetBestBlock (GAME_GENESIS_HEIGHT,
                               TestGame::GenesisBlockHash ());
  ReinitialiseState (g);

  /* Use another game instance (but with the same underlying storage) to
     simulate startup without a cached height (but persisted current game
     state).  */
  Game freshGame(GAME_ID);
  TestGame freshRules;
  freshGame.ConnectRpcClient (httpClient);
  freshGame.SetStorage (storage);
  freshGame.SetGameLogic (freshRules);
  ReinitialiseState (freshGame);

  const Json::Value state = freshGame.GetCurrentJsonState ();
  EXPECT_EQ (state["gameid"], GAME_ID);
  EXPECT_EQ (state["chain"], "main");
  EXPECT_EQ (state["state"], "up-to-date");
  EXPECT_EQ (state["blockhash"], GAME_GENESIS_HASH);
  EXPECT_EQ (state["height"].asInt (), 42);
  EXPECT_EQ (state["gamestate"]["state"], "");
}

/* ************************************************************************** */

class WaitForChangeTests : public InitialStateTests
{

private:

  /** The thread that is used to call WaitForChange.  */
  std::unique_ptr<std::thread> waiter;

protected:

  uint256 nullOldBlock;

  WaitForChangeTests ()
  {
    nullOldBlock.SetNull ();

    /* Since WaitForChange only really blocks when there is an active
       ZMQ subscriber, we need to set up a fake one.  So we can just use
       some address where hopefully no publishers are; we won't need
       actual notifications (as we fake them with explicit calls).  */

    const Json::Value notifications = ParseJson (R"(
      [
        {"type": "pubgameblocks", "address": "tcp://127.0.0.1:32101"}
      ]
    )");
    EXPECT_CALL (mockXayaServer, getzmqnotifications ())
        .WillOnce (Return (notifications));

    EXPECT_CALL (mockXayaServer, trackedgames (_, _)).Times (AnyNumber ());

    CHECK (g.DetectZmqEndpoint ());
    g.Start ();

    SetStartingBlock (TestGame::GenesisBlockHash ());
  }

  /**
   * Calls WaitForChange on a newly started thread, passing the given
   * uint256 output pointer (can be null).
   */
  void
  CallWaitForChange (const uint256& oldBlock, uint256& newBlock)
  {
    ASSERT_EQ (waiter, nullptr);
    waiter = std::make_unique<std::thread> ([this, &oldBlock, &newBlock] ()
      {
        g.WaitForChange (oldBlock, newBlock);
      });
  }

  /**
   * Verifies that a waiter has been started and received the notification
   * of a new state already (or waits for it to receive it).
   */
  void
  JoinWaiter ()
  {
    ASSERT_NE (waiter, nullptr);
    waiter->join ();
    waiter.reset ();
  }

};

TEST_F (WaitForChangeTests, ZmqNotRunning)
{
  g.Stop ();

  uint256 newBlock;
  CallWaitForChange (nullOldBlock, newBlock);
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, StopWakesUpWaiters)
{
  uint256 newBlock;
  CallWaitForChange (nullOldBlock, newBlock);
  g.Stop ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, InitialState)
{
  uint256 newBlock;
  CallWaitForChange (nullOldBlock, newBlock);
  SleepSome ();

  EXPECT_EQ (GetState (g), State::PREGENESIS);
  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);

  JoinWaiter ();
}

TEST_F (WaitForChangeTests, BlockAttach)
{
  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);

  uint256 newBlock;
  CallWaitForChange (nullOldBlock, newBlock);
  SleepSome ();
  AttachBlock (g, BlockHash (11), Moves (""));
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, BlockDetach)
{
  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  AttachBlock (g, BlockHash (11), Moves (""));

  uint256 newBlock;
  CallWaitForChange (nullOldBlock, newBlock);
  SleepSome ();
  DetachBlock (g);
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, ReturnsNoBestBlock)
{
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  uint256 newBlock;
  CallWaitForChange (BlockHash (42), newBlock);
  g.Stop ();
  JoinWaiter ();

  EXPECT_TRUE (newBlock.IsNull ());
}

TEST_F (WaitForChangeTests, ReturnsBestBlock)
{
  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);

  uint256 newBlock;
  CallWaitForChange (nullOldBlock, newBlock);
  g.Stop ();
  JoinWaiter ();

  EXPECT_FALSE (newBlock.IsNull ());
  EXPECT_TRUE (newBlock == TestGame::GenesisBlockHash ());
}

TEST_F (WaitForChangeTests, UpToDateOldBlock)
{
  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);

  uint256 newBlock;
  CallWaitForChange (TestGame::GenesisBlockHash (), newBlock);
  SleepSome ();
  AttachBlock (g, BlockHash (11), Moves (""));
  JoinWaiter ();

  EXPECT_FALSE (newBlock.IsNull ());
  EXPECT_TRUE (newBlock == BlockHash (11));
}

TEST_F (WaitForChangeTests, OutdatedOldBlock)
{
  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  AttachBlock (g, BlockHash (11), Moves (""));

  uint256 newBlock;
  CallWaitForChange (TestGame::GenesisBlockHash (), newBlock);
  SleepSome ();
  JoinWaiter ();

  EXPECT_FALSE (newBlock.IsNull ());
  EXPECT_TRUE (newBlock == BlockHash (11));
}

/* ************************************************************************** */

class SyncingTests : public InitialStateTests
{

protected:

  SyncingTests()
  {
    mockXayaServer.SetBestBlock (GAME_GENESIS_HEIGHT,
                                 TestGame::GenesisBlockHash ());
    SetStartingBlock (TestGame::GenesisBlockHash ());
    ReinitialiseState (g);
    EXPECT_EQ (GetState (g), State::UP_TO_DATE);
    ExpectGameState (TestGame::GenesisBlockHash (), "");
  }

  static void
  ExpectGameState (const StorageInterface& s,
                   const uint256& expectedHash,
                   const GameStateData& state)
  {
    uint256 hash;
    ASSERT_TRUE (s.GetCurrentBlockHash (hash));
    EXPECT_EQ (hash, expectedHash);
    EXPECT_EQ (s.GetCurrentGameState (), state);
  }

  void
  ExpectGameState (const uint256& expectedHash,
                   const GameStateData& state) const
  {
    ExpectGameState (storage, expectedHash, state);
  }

  /**
   * Utility method to construct a JSON response object for game_sendupdates.
   */
  static Json::Value
  SendupdatesResponse (const uint256& toblock, const std::string& reqtoken)
  {
    Json::Value res(Json::objectValue);
    res["toblock"] = toblock.ToHex ();
    res["reqtoken"] = reqtoken;
    return res;
  }

};

TEST_F (SyncingTests, UpToDateOperation)
{
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "a2b1c3");

  DetachBlock (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  DetachBlock (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (TestGame::GenesisBlockHash (), "");
}

TEST_F (SyncingTests, UpToDateIgnoresReqtoken)
{
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  /* Attach ignored because of its reqtoken.  */
  CallBlockAttach (g, "foo", BlockHash (11), BlockHash (12), 12,
                   Moves ("a5"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  /* Detach ignored because of its reqtoken.  */
  CallBlockDetach (g, "foo", BlockHash (11), BlockHash (12), 12,
                   NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "a2b1c3");
}

TEST_F (SyncingTests, CatchingUpForward)
{
  EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (12), "reqtoken")));

  mockXayaServer.SetBestBlock (12, BlockHash (12));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");

  CallBlockAttach (g, "reqtoken",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   Moves ("a0b1"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  /* Attach ignored because of its reqtoken.  */
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (11), BlockHash (12), 12,
                   Moves ("a5"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  /* Attach ignored because of its reqtoken.  */
  CallBlockAttach (g, "other req", BlockHash (1), BlockHash (2), 2,
                   Moves ("a6"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  /* Detach ignored because of its reqtoken.  */
  CallBlockDetach (g, NO_REQ_TOKEN, BlockHash (11), BlockHash (12), 12,
                   NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  CallBlockAttach (g, "reqtoken", BlockHash (11), BlockHash (12), 12,
                   Moves ("a2c3"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "a2b1c3");
}

TEST_F (SyncingTests, CatchingUpBackwards)
{
  EXPECT_CALL (mockXayaServer,
               game_sendupdates (BlockHash (12).ToHex (), GAME_ID))
      .WillOnce (Return (SendupdatesResponse (TestGame::GenesisBlockHash (),
                                              "reqtoken")));

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "a2b1c3");

  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (12), "a2b1c3");

  CallBlockDetach (g, "reqtoken", BlockHash (11), BlockHash (12), 12,
                   NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  CallBlockDetach (g, "reqtoken",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (TestGame::GenesisBlockHash (), "");
}

TEST_F (SyncingTests, CatchingUpMultistep)
{
  /* Tests the situation where a single game_sendupdates call is not enough
     to bring the game state fully up to date.  Xaya Core's
     -maxgameblockattaches limit is one reason why this may happen
     (https://github.com/xaya/xaya/pull/66).  */

  EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (12), "token 1")));
  EXPECT_CALL (mockXayaServer,
               game_sendupdates (BlockHash (12).ToHex (), GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (13), "token 2")));

  mockXayaServer.SetBestBlock (13, BlockHash (13));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");

  CallBlockAttach (g, "token 1",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   Moves ("a0b1"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  CallBlockAttach (g, "token 1", BlockHash (11), BlockHash (12), 12,
                   Moves ("a2c3"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (12), "a2b1c3");

  CallBlockAttach (g, "token 2", BlockHash (12), BlockHash (13), 13,
                   Moves ("a7"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (13), "a7b1c3");
}

TEST_F (SyncingTests, MissedAttachWhileUpToDate)
{
  EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (20), "reqtoken")));

  mockXayaServer.SetBestBlock (20, BlockHash (20));
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (19), BlockHash (20), 20,
                   Moves ("a1"), SEQ_MISMATCH);

  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");
}

TEST_F (SyncingTests, MissedDetachWhileUpToDate)
{
  EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (20), "reqtoken")));

  mockXayaServer.SetBestBlock (20, BlockHash (20));
  CallBlockDetach (g, NO_REQ_TOKEN, BlockHash (19), BlockHash (20), 20,
                   SEQ_MISMATCH);

  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");
}

TEST_F (SyncingTests, MissedAttachWhileCatchingUp)
{
  {
    InSequence dummy;
    EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
        .WillOnce (Return (SendupdatesResponse (BlockHash (12), "a")));
    EXPECT_CALL (mockXayaServer,
                 game_sendupdates (BlockHash (11).ToHex (), GAME_ID))
        .WillOnce (Return (SendupdatesResponse (BlockHash (12), "b")));
  }

  mockXayaServer.SetBestBlock (12, BlockHash (12));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");

  CallBlockAttach (g, "a", TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   Moves ("a0b1"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  /* This attach with a sequence mismatch triggers another reinitialisation,
     so that we make the second game_sendupdates call and from then on wait
     for the "b" reqtoken.  */
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (12), BlockHash (13), 13,
                   Moves ("a5"), SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  CallBlockAttach (g, "b", BlockHash (11), BlockHash (12), 12,
                   Moves ("a2c3"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "a2b1c3");
}

/* ************************************************************************** */

class PruningTests : public SyncingTests
{

protected:

  PruningTests ()
  {
    /* For the tests, we keep the last block.  This enables us to verify that
       something is kept and do undos, but it also makes it easy to verify
       that stuff gets pruned quickly.  */
    g.EnablePruning (1);
  }

  /**
   * Verifies that the undo data for the given hash is pruned.
   */
  void
  AssertIsPruned (const uint256& hash) const
  {
    UndoData dummyUndo;
    ASSERT_FALSE (storage.GetUndoData (hash, dummyUndo));
  }

  /**
   * Verifies that the undo data for the given hash is not pruned.
   */
  void
  AssertNotPruned (const uint256& hash) const
  {
    UndoData dummyUndo;
    ASSERT_TRUE (storage.GetUndoData (hash, dummyUndo));
  }

};

TEST_F (PruningTests, AttachDetach)
{
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");
  AssertIsPruned (TestGame::GenesisBlockHash ());
  AssertNotPruned (BlockHash (11));

  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "a2b1c3");
  AssertIsPruned (BlockHash (11));
  AssertNotPruned (BlockHash (12));

  /* Detaching one block should work, as we keep one undo state.  */
  DetachBlock (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");
}

TEST_F (PruningTests, WithReqToken)
{
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");
  AssertIsPruned (TestGame::GenesisBlockHash ());
  AssertNotPruned (BlockHash (11));

  CallBlockAttach (g, "foo", BlockHash (11), BlockHash (12), 12,
                   Moves ("a2c3"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");
  AssertNotPruned (BlockHash (11));
}

TEST_F (PruningTests, MissedZmq)
{
  EXPECT_CALL (mockXayaServer, game_sendupdates (_, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (12), "reqtoken")));

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");
  AssertIsPruned (TestGame::GenesisBlockHash ());
  AssertNotPruned (BlockHash (11));

  /* This will trigger a game_sendupdates and bring the state to catching-up,
     but we don't care about it.  It should, most of all, not prune the last
     block as it would without sequence mismatch.  */
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (12), BlockHash (13), 13,
                   Moves (""), SEQ_MISMATCH);
  AssertNotPruned (BlockHash (11));
}

/* ************************************************************************** */

/**
 * Helper subclass of MemoryStorage that allows us to fail (throw an exception)
 * when setting the current state.
 */
class FallibleMemoryStorage : public TxMockedMemoryStorage
{

private:

  /** Whether or not SetCurrentGameState should fail.  */
  bool shouldFail = false;

public:

  class Failure : public std::runtime_error
  {

  public:

    Failure ()
      : std::runtime_error("failing memory storage")
    {}

  };

  /**
   * Sets whether or not SetCurrentGameState calls should fail (throw an
   * exception) when called instead of changing the game state.
   */
  void
  SetShouldFail (const bool val)
  {
    shouldFail = val;
  }

  void
  SetCurrentGameState (const uint256& hash, const GameStateData& data) override
  {
    if (shouldFail)
      {
        LOG (INFO) << "Failing call to SetCurrentGameState on purpose";
        throw Failure ();
      }
    MemoryStorage::SetCurrentGameState (hash, data);
  }

};

class GameLogicTransactionsTests : public SyncingTests
{

protected:

  /** Alternative (fallible) storage used in the tests.  */
  FallibleMemoryStorage fallibleStorage;

  GameLogicTransactionsTests ()
  {
    LOG (INFO) << "Changing game to fallible storage";
    g.SetStorage (fallibleStorage);

    ReinitialiseState (g);
    EXPECT_EQ (GetState (g), State::UP_TO_DATE);
    ExpectGameState (storage, TestGame::GenesisBlockHash (), "");
  }

};

TEST_F (GameLogicTransactionsTests, UpToDate)
{
  {
    InSequence dummy;

    EXPECT_CALL (fallibleStorage, RollbackTransactionMock ()).Times (0);

    EXPECT_CALL (fallibleStorage, BeginTransactionMock ());
    EXPECT_CALL (fallibleStorage, CommitTransactionMock ());

    EXPECT_CALL (fallibleStorage, BeginTransactionMock ());
    EXPECT_CALL (fallibleStorage, CommitTransactionMock ());
  }

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (fallibleStorage, BlockHash (11), "a0b1");

  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (fallibleStorage, BlockHash (12), "a2b1c3");
}

TEST_F (GameLogicTransactionsTests, CatchingUpBatched)
{
  {
    InSequence dummy;

    EXPECT_CALL (fallibleStorage, RollbackTransactionMock ()).Times (0);

    EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
        .WillOnce (Return (SendupdatesResponse (BlockHash (12), "reqtoken")));

    EXPECT_CALL (fallibleStorage, BeginTransactionMock ());
    EXPECT_CALL (fallibleStorage, CommitTransactionMock ());
  }

  mockXayaServer.SetBestBlock (12, BlockHash (12));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (storage, TestGame::GenesisBlockHash (), "");

  CallBlockAttach (g, "reqtoken",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   Moves ("a0b1"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (fallibleStorage, BlockHash (11), "a0b1");

  CallBlockAttach (g, "reqtoken", BlockHash (11), BlockHash (12), 12,
                   Moves ("a2c3"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (fallibleStorage, BlockHash (12), "a2b1c3");
}

TEST_F (GameLogicTransactionsTests, FailureRollsBack)
{
  {
    InSequence dummy;
    EXPECT_CALL (fallibleStorage, CommitTransactionMock ()).Times (0);
    EXPECT_CALL (fallibleStorage, BeginTransactionMock ());
    EXPECT_CALL (fallibleStorage, RollbackTransactionMock ());
  }

  fallibleStorage.SetShouldFail (true);

  try
    {
      AttachBlock (g, BlockHash (11), Moves ("a0b1"));
      FAIL () << "No failure thrown from memory storage";
    }
  catch (const FallibleMemoryStorage::Failure& exc)
    {
      LOG (INFO) << "Caught expected memory failure";
    }

  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (fallibleStorage, TestGame::GenesisBlockHash (), "");
}

/* ************************************************************************** */

/**
 * MemoryStorage instance that can be made to fail with RetryWithNewTransaction
 * so that we can test the retry logic in Game.
 */
class RetryMemoryStorage : public MemoryStorage
{

private:

  /** Number of retry failures that have been thrown.  */
  unsigned numFailures = 0;

  /**
   * Whether or not to fail the next SetCurrentGameState with a retry
   * request.  The flag will always be set to false after a failure,
   * so that the retry actually works.
   */
  bool retryNext = false;

public:

  RetryMemoryStorage () = default;

  unsigned
  GetNumFailures () const
  {
    return numFailures;
  }

  void
  RetryNext ()
  {
    ASSERT_FALSE (retryNext);
    LOG (INFO) << "Will fail next update with RetryWithNewTransaction";
    retryNext = true;
  }

  void
  SetCurrentGameState (const uint256& hash, const GameStateData& state) override
  {
    if (retryNext)
      {
        ++numFailures;
        retryNext = false;
        LOG (INFO) << "Failing update for the " << numFailures << "th time";
        throw StorageInterface::RetryWithNewTransaction ("retry commit");
      }

    MemoryStorage::SetCurrentGameState (hash, state);
  }

};

class GameStorageRetryTests : public SyncingTests
{

protected:

  /** Alternative, "retryable" storage used in the tests.  */
  RetryMemoryStorage retryStorage;

  GameStorageRetryTests ()
  {
    LOG (INFO) << "Changing game to retry storage";
    g.SetStorage (retryStorage);

    ReinitialiseState (g);
    EXPECT_EQ (GetState (g), State::UP_TO_DATE);
    ExpectGameState (retryStorage, TestGame::GenesisBlockHash (), "");
  }

};

TEST_F (GameStorageRetryTests, InitialState)
{
  /* The test fixture constructor already sets the initial state.  So in order
     to make sure it is actually committed to the database below, clear the
     storage now.  */
  retryStorage.Clear ();

  EXPECT_EQ (retryStorage.GetNumFailures (), 0);
  retryStorage.RetryNext ();
  ReinitialiseState (g);
  EXPECT_EQ (retryStorage.GetNumFailures (), 1);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (retryStorage, TestGame::GenesisBlockHash (), "");
}

TEST_F (GameStorageRetryTests, AttachBlock)
{
  EXPECT_CALL (mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (11), "reqtoken")));
  mockXayaServer.SetBestBlock (11, BlockHash (11));

  EXPECT_EQ (retryStorage.GetNumFailures (), 0);
  retryStorage.RetryNext ();
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (retryStorage.GetNumFailures (), 1);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (retryStorage, TestGame::GenesisBlockHash (), "");

  CallBlockAttach (g, "reqtoken",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   Moves ("a0b1"), NO_SEQ_MISMATCH);
  EXPECT_EQ (retryStorage.GetNumFailures (), 1);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (retryStorage, BlockHash (11), "a0b1");

}

TEST_F (GameStorageRetryTests, DetachBlock)
{
  EXPECT_CALL (mockXayaServer,
               game_sendupdates (BlockHash (11).ToHex (), GAME_ID))
      .WillOnce (Return (SendupdatesResponse (TestGame::GenesisBlockHash (),
                                              "reqtoken")));
  mockXayaServer.SetBestBlock (10, TestGame::GenesisBlockHash ());

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (retryStorage.GetNumFailures (), 0);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (retryStorage, BlockHash (11), "a0b1");

  retryStorage.RetryNext ();
  DetachBlock (g);
  EXPECT_EQ (retryStorage.GetNumFailures (), 1);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (retryStorage, BlockHash (11), "a0b1");

  CallBlockDetach (g, "reqtoken",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   NO_SEQ_MISMATCH);
  EXPECT_EQ (retryStorage.GetNumFailures (), 1);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (retryStorage, TestGame::GenesisBlockHash (), "");

}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
