// Copyright (C) 2018-2024 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include "gamelogic.hpp"

#include "testutils.hpp"

#include "rpc-stubs/xayarpcserverstub.h"

#include <xayautil/hash.hpp>
#include <xayautil/uint256.hpp>

#include <json/json.h>
#include <jsonrpccpp/common/exception.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <thread>

DECLARE_int32 (xaya_zmq_staleness_ms);

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
 * getblockchaininfo) have an explicit fake implementation.
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
   * The genesis hash reported.  This is GAME_GENESIS_HASH by default, but
   * can be changed (e.g. to "" for testing unspecified genesis hash)
   * in specific tests.
   */
  std::string genesisHash = GAME_GENESIS_HASH;

  /** Mutex locking lastInstanceState.  */
  mutable std::mutex mutInstanceState;
  /** The last state passed to the InstanceStateChanged() notification.  */
  Json::Value lastInstanceState;

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
    hashHex = genesisHash;
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

  void
  InstanceStateChanged (const Json::Value& state) override
  {
    std::lock_guard<std::mutex> lock(mutInstanceState);
    lastInstanceState = state;
  }

  Json::Value
  GetLastInstanceState () const
  {
    std::lock_guard<std::mutex> lock(mutInstanceState);
    /* Return a copy (by value) on purpose to make sure the caller can then
       use the object without worrying about race conditions and other
       threads / locking.  */
    return lastInstanceState;
  }

  void
  SetGenesisHash (const std::string& h)
  {
    genesisHash = h;
  }

  static uint256
  GenesisBlockHash ()
  {
    uint256 res;
    CHECK (res.FromHex (GAME_GENESIS_HASH));
    return res;
  }

};

/**
 * Processor for pending moves of our test game.  The JSON state returned
 * for the pending moves is just a JSON object where names are mapped to
 * the latest value in a move (i.e. what the value would be when all
 * transactions are confirmed).
 *
 * In addition to the names, we also add the current state as string
 * to the JSON object with key "state".
 */
class TestPendingMoves : public PendingMoveProcessor
{

private:

  /** The currently built up JSON object.  */
  Json::Value data;

protected:

  void
  Clear () override
  {
    data = Json::Value (Json::objectValue);
  }

  void
  AddPendingMove (const Json::Value& mv) override
  {
    data["state"] = GetConfirmedState ();
    data["height"] = GetConfirmedBlock ()["height"].asInt ();

    const std::string nm = mv["name"].asString ();
    const std::string val = mv["move"].asString ();
    data[nm] = val;
  }

public:

  TestPendingMoves ()
    : data(Json::objectValue)
  {}

  Json::Value
  ToJson () const override
  {
    return data;
  }

};

/* ************************************************************************** */

class GameTests : public GameTestWithBlockchain
{

protected:

  HttpRpcServer<MockXayaRpcServerWithState> mockXayaServer;

  /** In-memory storage that can be used in tests.  */
  MemoryStorage storage;
  /** Game rules for the test game.  */
  TestGame rules;

  GameTests ()
    : GameTestWithBlockchain(GAME_ID)
  {
    /* The mocked RPC server listens on separate threads and is already set up
       (cannot be started only from within the death test), so we need to run
       those threadsafe.  */
    testing::FLAGS_gtest_death_test_style = "threadsafe";
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

  /**
   * Connects the given Game instance to our mock server.
   */
  void
  ConnectToMockRpc (Game& g)
  {
    g.ConnectRpcClient (mockXayaServer.GetClientConnector (),
                        jsonrpc::JSONRPC_CLIENT_V2);
  }

};

/* ************************************************************************** */

using XayaVersionTests = GameTests;

TEST_F (XayaVersionTests, Works)
{
  Json::Value networkInfo(Json::objectValue);
  networkInfo["version"] = 1020300;
  EXPECT_CALL (*mockXayaServer, getnetworkinfo ())
      .WillOnce (Return (networkInfo));

  Game g(GAME_ID);
  ConnectToMockRpc (g);
  EXPECT_EQ (g.GetXayaVersion (), 1020300);
}

/* ************************************************************************** */

using ChainDetectionTests = GameTests;

TEST_F (ChainDetectionTests, ChainDetected)
{
  Game g(GAME_ID);
  mockXayaServer->SetBestBlock (0, BlockHash (0));
  ConnectToMockRpc (g);
  EXPECT_TRUE (g.GetChain () == Chain::MAIN);
}

TEST_F (ChainDetectionTests, Reconnection)
{
  /* For the death test, we need to make sure that we only run the server
     in the forked environment.  If we set up the mock expectations before
     forking, they will be set in both processes, but only fulfilled
     in one of them.  */
  mockXayaServer->StopListening ();

  Game g(GAME_ID);
  EXPECT_DEATH (
    {
      mockXayaServer->StartListening ();
      mockXayaServer->SetBestBlock (0, BlockHash (0));
      ConnectToMockRpc (g);
      ConnectToMockRpc (g);
    },
    "RPC client is already connected");
}

/* ************************************************************************** */

using DetectZmqEndpointTests = GameTests;

TEST_F (DetectZmqEndpointTests, BlocksWithoutPending)
{
  const Json::Value notifications = ParseJson (R"(
    [
      {"address": "foobar"},
      {"type": "sometype", "address": "someaddress"},
      {"type": "pubgameblocks", "address": "address"}
    ]
  )");

  EXPECT_CALL (*mockXayaServer, getzmqnotifications ())
      .WillOnce (Return (notifications));

  Game g(GAME_ID);
  mockXayaServer->SetBestBlock (0, BlockHash (0));
  ConnectToMockRpc (g);
  ASSERT_TRUE (g.DetectZmqEndpoint ());
  EXPECT_EQ (GetZmqEndpoint (g), "address");
  EXPECT_EQ (GetZmqEndpointPending (g), "");
}

TEST_F (DetectZmqEndpointTests, BlocksAndPending)
{
  const Json::Value notifications = ParseJson (R"(
    [
      {"type": "pubgameblocks", "address": "address blocks"},
      {"type": "pubgamepending", "address": "address pending"}
    ]
  )");

  EXPECT_CALL (*mockXayaServer, getzmqnotifications ())
      .WillOnce (Return (notifications));

  Game g(GAME_ID);
  mockXayaServer->SetBestBlock (0, BlockHash (0));
  ConnectToMockRpc (g);
  ASSERT_TRUE (g.DetectZmqEndpoint ());
  EXPECT_EQ (GetZmqEndpoint (g), "address blocks");
  EXPECT_EQ (GetZmqEndpointPending (g), "address pending");
}

TEST_F (DetectZmqEndpointTests, NotSet)
{
  const Json::Value notifications = ParseJson (R"(
    [
      {"address": "foobar"},
      {"type": "sometype", "address": "someaddress"}
    ]
  )");

  EXPECT_CALL (*mockXayaServer, getzmqnotifications ())
      .WillOnce (Return (notifications));

  Game g(GAME_ID);
  mockXayaServer->SetBestBlock (0, BlockHash (0));
  ConnectToMockRpc (g);
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
    EXPECT_CALL (*mockXayaServer, trackedgames ("add", GAME_ID));
    EXPECT_CALL (*mockXayaServer, trackedgames ("remove", GAME_ID));
  }

  Game g(GAME_ID);
  mockXayaServer->SetBestBlock (0, BlockHash (0));
  ConnectToMockRpc (g);
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

  InitialStateTests ()
    : g(GAME_ID)
  {
    EXPECT_CALL (*mockXayaServer, getblockhash (GAME_GENESIS_HEIGHT))
        .WillRepeatedly (Return (GAME_GENESIS_HASH));

    mockXayaServer->SetBestBlock (0, BlockHash (0));
    ConnectToMockRpc (g);

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
   * would appear in the block notification.  The txid of the move is derived
   * by hashing the name.
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
        const std::string val = str.substr (i + 1, 1);
        obj["move"] = val;
        obj["txid"] = SHA256::Hash (val).ToHex ();
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
  EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (upd));

  mockXayaServer->SetBestBlock (20, BlockHash (20));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, WaitingForGenesis)
{
  const Json::Value emptyMoves(Json::objectValue);

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer->SetBestBlock (9, BlockHash (9));
  SetStartingBlock (8, BlockHash (8));
  AttachBlock (g, BlockHash (9), emptyMoves);
  EXPECT_EQ (GetState (g), State::PREGENESIS);
  EXPECT_EQ (rules.GetLastInstanceState ()["state"].asString (), "pregenesis");

  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
  AttachBlock (g, TestGame::GenesisBlockHash (), emptyMoves);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_EQ (rules.GetLastInstanceState ()["state"].asString (), "up-to-date");
  EXPECT_EQ (rules.GetLastInstanceState ()["height"].asInt64 (), 10);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, MissedNotification)
{
  const Json::Value emptyMoves(Json::objectValue);

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer->SetBestBlock (9, BlockHash (9));
  SetStartingBlock (8, BlockHash (8));
  AttachBlock (g, BlockHash (9), emptyMoves);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer->SetBestBlock (20, TestGame::GenesisBlockHash ());
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (19), BlockHash (20), 20,
                   emptyMoves, SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectInitialStateInStorage ();
}

TEST_F (InitialStateTests, MismatchingGenesisHash)
{
  EXPECT_CALL (*mockXayaServer, getblockhash (GAME_GENESIS_HEIGHT))
      .WillRepeatedly (Return (std::string (64, '0')));

  mockXayaServer->SetBestBlock (20, BlockHash (20));
  EXPECT_DEATH (
      ReinitialiseState (g),
      "genesis block hash and height do not match");
}

TEST_F (InitialStateTests, UnspecifiedGenesisHash)
{
  EXPECT_CALL (*mockXayaServer, getblockhash (GAME_GENESIS_HEIGHT))
      .WillRepeatedly (Return (BlockHash (10).ToHex ()));

  const Json::Value emptyMoves(Json::objectValue);
  rules.SetGenesisHash ("");

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer->SetBestBlock (9, BlockHash (9));
  SetStartingBlock (8, BlockHash (8));
  AttachBlock (g, BlockHash (9), emptyMoves);
  EXPECT_EQ (GetState (g), State::PREGENESIS);

  mockXayaServer->SetBestBlock (10, BlockHash (10));
  AttachBlock (g, BlockHash (10), emptyMoves);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);

  uint256 hash;
  EXPECT_TRUE (storage.GetCurrentBlockHash (hash));
  EXPECT_EQ (hash, BlockHash (10));
}

/* ************************************************************************** */

using GetCurrentJsonStateTests = InitialStateTests;

TEST_F (GetCurrentJsonStateTests, NoStateYet)
{
  const Json::Value state = g.GetCurrentJsonState ();
  EXPECT_EQ (state["gameid"], GAME_ID);
  EXPECT_EQ (state["chain"], "main");
  EXPECT_EQ (state["state"], "disconnected");
  EXPECT_FALSE (state.isMember ("blockhash"));
  EXPECT_FALSE (state.isMember ("height"));
  EXPECT_FALSE (state.isMember ("gamestate"));
}

TEST_F (GetCurrentJsonStateTests, InitialState)
{
  mockXayaServer->SetBestBlock (GAME_GENESIS_HEIGHT,
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
  mockXayaServer->SetBestBlock (GAME_GENESIS_HEIGHT,
                                TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  SetStartingBlock (GAME_GENESIS_HEIGHT, TestGame::GenesisBlockHash ());
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));

  const Json::Value state = g.GetCurrentJsonState ();
  EXPECT_EQ (state["gameid"], GAME_ID);
  EXPECT_EQ (state["chain"], "main");
  EXPECT_EQ (state["state"], "up-to-date");
  EXPECT_EQ (state["blockhash"], BlockHash (11).ToHex ());
  EXPECT_EQ (state["height"].asInt (), 11);
  EXPECT_EQ (state["gamestate"]["state"], "a0b1");
}

TEST_F (GetCurrentJsonStateTests, HeightResolvedViaRpc)
{
  Json::Value blockHeaderData(Json::objectValue);
  blockHeaderData["height"] = 42;
  EXPECT_CALL (*mockXayaServer, getblockheader (GAME_GENESIS_HASH))
      .WillOnce (Return (blockHeaderData));

  mockXayaServer->SetBestBlock (GAME_GENESIS_HEIGHT,
                                TestGame::GenesisBlockHash ());
  ReinitialiseState (g);

  /* Use another game instance (but with the same underlying storage) to
     simulate startup without a cached height (but persisted current game
     state).  */
  Game freshGame(GAME_ID);
  TestGame freshRules;
  ConnectToMockRpc (freshGame);
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

TEST_F (GetCurrentJsonStateTests, CallbackUnblocked)
{
  mockXayaServer->SetBestBlock (GAME_GENESIS_HEIGHT,
                                TestGame::GenesisBlockHash ());
  ReinitialiseState (g);

  std::atomic<bool> firstStarted;
  std::atomic<bool> firstDone;
  firstStarted = false;
  firstDone = false;

  std::thread first([&] ()
    {
      g.GetCustomStateData ("data",
          [&] (const GameStateData& state)
          {
            LOG (INFO) << "Long callback started";
            firstStarted = true;
            std::this_thread::sleep_for (std::chrono::milliseconds (100));
            LOG (INFO) << "Long callback done";
            return Json::Value ();
          });
      firstDone = true;
    });

  while (!firstStarted)
    std::this_thread::sleep_for (std::chrono::milliseconds (1));

  g.GetCustomStateData ("data",
      [&] (const GameStateData& state)
      {
        LOG (INFO) << "Second (short) callback";
        return Json::Value ();
      });

  EXPECT_FALSE (firstDone);
  first.join ();
}

/* ************************************************************************** */

class GetPendingJsonStateTests : public InitialStateTests
{

protected:

  GetPendingJsonStateTests ()
  {
    EXPECT_CALL (*mockXayaServer, trackedgames (_, _)).Times (AnyNumber ());
    EXPECT_CALL (*mockXayaServer, getrawmempool ())
        .WillRepeatedly (Return (Json::Value (Json::arrayValue)));
    SetStartingBlock (GAME_GENESIS_HEIGHT, TestGame::GenesisBlockHash ());
  }

  /**
   * Sets up the ZMQ endpoints (by mocking getzmqnotifications in the RPC
   * server and detecting them).  This can either make the server expose
   * a notification for pending moves or not.
   */
  void
  SetupZmqEndpoints (const bool withPending)
  {
    Json::Value notifications = ParseJson (R"(
      [
        {"type": "pubgameblocks", "address": "tcp://127.0.0.1:32101"}
      ]
    )");

    if (withPending)
      notifications.append (ParseJson (R"(
        {"type": "pubgamepending", "address": "tcp://127.0.0.1:32102"}
      )"));

    EXPECT_CALL (*mockXayaServer, getzmqnotifications ())
        .WillOnce (Return (notifications));
    CHECK (g.DetectZmqEndpoint ());
  }

};

TEST_F (GetPendingJsonStateTests, NoAttachedProcessor)
{
  SetupZmqEndpoints (true);
  g.Start ();

  EXPECT_THROW (g.GetPendingJsonState (), jsonrpc::JsonRpcException);
}

TEST_F (GetPendingJsonStateTests, PendingNotificationDisabled)
{
  TestPendingMoves proc;
  g.SetPendingMoveProcessor (proc);

  SetupZmqEndpoints (false);
  g.Start ();

  EXPECT_THROW (g.GetPendingJsonState (), jsonrpc::JsonRpcException);
}

TEST_F (GetPendingJsonStateTests, PendingState)
{
  TestPendingMoves proc;
  g.SetPendingMoveProcessor (proc);

  SetupZmqEndpoints (true);
  g.Start ();

  mockXayaServer->SetBestBlock (GAME_GENESIS_HEIGHT,
                                TestGame::GenesisBlockHash ());
  ReinitialiseState (g);

  AttachBlock (g, BlockHash (11), Moves (""));
  CallPendingMove (g, Moves ("ax")[0]);

  const auto state = g.GetPendingJsonState ();
  EXPECT_EQ (state["gameid"], GAME_ID);
  EXPECT_EQ (state["chain"], "main");
  EXPECT_EQ (state["state"], "up-to-date");
  EXPECT_EQ (state["blockhash"], BlockHash (11).ToHex ());
  EXPECT_EQ (state["pending"], ParseJson (R"(
    {
      "state": "",
      "height": 11,
      "a": "x"
    }
  )"));
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
    EXPECT_CALL (*mockXayaServer, getzmqnotifications ())
        .WillOnce (Return (notifications));

    EXPECT_CALL (*mockXayaServer, trackedgames (_, _)).Times (AnyNumber ());

    CHECK (g.DetectZmqEndpoint ());
    g.Start ();

    SetStartingBlock (GAME_GENESIS_HEIGHT, TestGame::GenesisBlockHash ());
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
  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);

  JoinWaiter ();
}

TEST_F (WaitForChangeTests, BlockAttach)
{
  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
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
  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
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
  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
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
  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
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
  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
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

class WaitForPendingChangeTests : public GetPendingJsonStateTests
{

private:

  /** The thread that is used to call WaitForPendingChange.  */
  std::unique_ptr<std::thread> waiter;

  TestPendingMoves proc;

protected:

  /** Flag that tells us when the waiter thread is done.  */
  std::atomic<bool> waiterDone;

  WaitForPendingChangeTests ()
  {
    g.SetPendingMoveProcessor (proc);
    SetStartingBlock (GAME_GENESIS_HEIGHT, TestGame::GenesisBlockHash ());

    mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
    ReinitialiseState (g);
    EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  }

  /**
   * Calls WaitForPendingChange on a newly started thread, storing the output
   * to the given variable.  If the method throws a JsonRpcException, then
   * the output is set to null.
   */
  void
  CallWaitForPendingChange (const int oldVersion, Json::Value& output)
  {
    ASSERT_EQ (waiter, nullptr);
    waiterDone = false;
    waiter = std::make_unique<std::thread> ([this, oldVersion, &output] ()
      {
        try
          {
            output = g.WaitForPendingChange (oldVersion);
          }
        catch (const jsonrpc::JsonRpcException& exc)
          {
            LOG (INFO)
                << "Caught exception from WaitForPendingChange: "
                << exc.what ();
            output = Json::Value ();
          }
        waiterDone = true;
      });
  }

  /**
   * Verifies that a waiter has been started and received the notification
   * of a new state already, or does so soon.
   */
  void
  JoinWaiter ()
  {
    ASSERT_NE (waiter, nullptr);
    if (!waiterDone)
      SleepSome ();
    EXPECT_TRUE (waiterDone);
    waiter->join ();
    waiter.reset ();
  }

};

TEST_F (WaitForPendingChangeTests, ZmqNotRunning)
{
  SetupZmqEndpoints (true);

  Json::Value out;
  CallWaitForPendingChange (Game::WAITFORCHANGE_ALWAYS_BLOCK, out);
  JoinWaiter ();
  EXPECT_TRUE (out.isObject ());
}

TEST_F (WaitForPendingChangeTests, NotTrackingPendingMoves)
{
  SetupZmqEndpoints (false);
  g.Start ();
  AttachBlock (g, BlockHash (11), Moves (""));

  Json::Value out;
  CallWaitForPendingChange (Game::WAITFORCHANGE_ALWAYS_BLOCK, out);
  JoinWaiter ();

  /* When pending moves are not tracked, then WaitForPendingChange should
     throw a JSON-RPC error just like GetPendingJsonState.  */
  EXPECT_TRUE (out.isNull ());
}

TEST_F (WaitForPendingChangeTests, StopWakesUpWaiters)
{
  SetupZmqEndpoints (true);
  g.Start ();
  AttachBlock (g, BlockHash (11), Moves (""));

  Json::Value out;
  CallWaitForPendingChange (Game::WAITFORCHANGE_ALWAYS_BLOCK, out);
  SleepSome ();
  EXPECT_FALSE (waiterDone);
  g.Stop ();
  JoinWaiter ();
  EXPECT_TRUE (out.isObject ());
}

TEST_F (WaitForPendingChangeTests, OldVersionImmediateReturn)
{
  SetupZmqEndpoints (true);
  g.Start ();
  AttachBlock (g, BlockHash (11), Moves (""));

  const auto state = g.GetPendingJsonState ();

  Json::Value out;
  CallWaitForPendingChange (42, out);
  JoinWaiter ();
  EXPECT_EQ (out, state);
}

TEST_F (WaitForPendingChangeTests, OldVersionWaiting)
{
  SetupZmqEndpoints (true);
  g.Start ();
  AttachBlock (g, BlockHash (11), Moves (""));

  const int oldVersion = g.GetPendingJsonState ()["version"].asInt ();

  Json::Value out;
  CallWaitForPendingChange (oldVersion, out);

  SleepSome ();
  EXPECT_FALSE (waiterDone);

  CallPendingMove (g, Moves ("ax")[0]);
  JoinWaiter ();
}

TEST_F (WaitForPendingChangeTests, PendingMove)
{
  SetupZmqEndpoints (true);
  g.Start ();
  AttachBlock (g, BlockHash (11), Moves (""));

  Json::Value out;
  CallWaitForPendingChange (Game::WAITFORCHANGE_ALWAYS_BLOCK, out);

  SleepSome ();
  EXPECT_FALSE (waiterDone);

  CallPendingMove (g, Moves ("ax")[0]);
  JoinWaiter ();
  EXPECT_EQ (out["pending"], ParseJson (R"(
    {
      "state": "",
      "height": 11,
      "a": "x"
    }
  )"));
}

TEST_F (WaitForPendingChangeTests, AttachedBlock)
{
  SetupZmqEndpoints (true);
  g.Start ();
  AttachBlock (g, BlockHash (11), Moves (""));

  Json::Value out;
  CallWaitForPendingChange (Game::WAITFORCHANGE_ALWAYS_BLOCK, out);

  SleepSome ();
  EXPECT_FALSE (waiterDone);

  AttachBlock (g, BlockHash (12), Moves ("a0b1"));
  JoinWaiter ();
  EXPECT_EQ (out["pending"], ParseJson ("{}"));
}

TEST_F (WaitForPendingChangeTests, DetachedBlock)
{
  SetupZmqEndpoints (true);
  g.Start ();
  AttachBlock (g, BlockHash (11), Moves (""));

  AttachBlock (g, BlockHash (12), Moves ("a0b1"));

  Json::Value out;
  CallWaitForPendingChange (Game::WAITFORCHANGE_ALWAYS_BLOCK, out);

  SleepSome ();
  EXPECT_FALSE (waiterDone);

  DetachBlock (g);
  JoinWaiter ();
  EXPECT_EQ (out["pending"], ParseJson ("{}"));
}

/* ************************************************************************** */

class SyncingTests : public InitialStateTests
{

protected:

  SyncingTests ()
  {
    mockXayaServer->SetBestBlock (GAME_GENESIS_HEIGHT,
                                  TestGame::GenesisBlockHash ());
    SetStartingBlock (GAME_GENESIS_HEIGHT, TestGame::GenesisBlockHash ());
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
  EXPECT_EQ (rules.GetLastInstanceState ()["height"].asInt64 (), 11);
  ExpectGameState (BlockHash (11), "a0b1");

  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_EQ (rules.GetLastInstanceState ()["height"].asInt64 (), 12);
  ExpectGameState (BlockHash (12), "a2b1c3");

  DetachBlock (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_EQ (rules.GetLastInstanceState ()["height"].asInt64 (), 11);
  ExpectGameState (BlockHash (11), "a0b1");

  DetachBlock (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_EQ (rules.GetLastInstanceState ()["height"].asInt64 (), 10);
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
  EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (12), "reqtoken")));

  mockXayaServer->SetBestBlock (12, BlockHash (12));
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
  EXPECT_CALL (*mockXayaServer,
               game_sendupdates (BlockHash (12).ToHex (), GAME_ID))
      .WillOnce (Return (SendupdatesResponse (TestGame::GenesisBlockHash (),
                                              "reqtoken")));

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "a2b1c3");

  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());
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

  EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (12), "token 1")));
  EXPECT_CALL (*mockXayaServer,
               game_sendupdates (BlockHash (12).ToHex (), GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (13), "token 2")));

  mockXayaServer->SetBestBlock (13, BlockHash (13));
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
  EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (20), "reqtoken")));

  mockXayaServer->SetBestBlock (20, BlockHash (20));
  CallBlockAttach (g, NO_REQ_TOKEN, BlockHash (19), BlockHash (20), 20,
                   Moves ("a1"), SEQ_MISMATCH);

  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");
}

TEST_F (SyncingTests, MissedDetachWhileUpToDate)
{
  EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (20), "reqtoken")));

  mockXayaServer->SetBestBlock (20, BlockHash (20));
  CallBlockDetach (g, NO_REQ_TOKEN, BlockHash (19), BlockHash (20), 20,
                   SEQ_MISMATCH);

  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");
}

TEST_F (SyncingTests, MissedAttachWhileCatchingUp)
{
  {
    InSequence dummy;
    EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
        .WillOnce (Return (SendupdatesResponse (BlockHash (12), "a")));
    EXPECT_CALL (*mockXayaServer,
                 game_sendupdates (BlockHash (11).ToHex (), GAME_ID))
        .WillOnce (Return (SendupdatesResponse (BlockHash (12), "b")));
  }

  mockXayaServer->SetBestBlock (12, BlockHash (12));
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


TEST_F (SyncingTests, GameStateUpdatedNotifications)
{
  /**
   * Modified TestGame instance that mocks out GameStateUpdated so we can
   * use gmock expectations on it.
   */
  class : public TestGame
  {
  public:
    MOCK_METHOD2 (GameStateUpdated,
                  void (const GameStateData& state,
                        const Json::Value& blockData) override);
  } r;

  g.SetGameLogic (r);
  storage.Clear ();

  {
    InSequence dummy;

    Json::Value genesisBlock(Json::objectValue);
    genesisBlock["height"] = static_cast<Json::Int64> (GAME_GENESIS_HEIGHT);
    genesisBlock["hash"] = GAME_GENESIS_HASH;
    EXPECT_CALL (r, GameStateUpdated ("", genesisBlock)).Times (1);

    Json::Value attachedBlock(Json::objectValue);
    attachedBlock["height"] = 11u;
    attachedBlock["hash"] = BlockHash (11).ToHex ();
    attachedBlock["rngseed"] = BlockHash (11).ToHex ();
    attachedBlock["parent"] = GAME_GENESIS_HASH;
    EXPECT_CALL (r, GameStateUpdated ("a0b1", attachedBlock)).Times (1);

    EXPECT_CALL (r, GameStateUpdated ("", genesisBlock)).Times (1);
  }

  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (TestGame::GenesisBlockHash (), "");

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  DetachBlock (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (TestGame::GenesisBlockHash (), "");
}

/* ************************************************************************** */

using TargetBlockTests = SyncingTests;

TEST_F (TargetBlockTests, StopsWhileAttaching)
{
  SetTargetBlock (g, BlockHash (12));

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  AttachBlock (g, BlockHash (12), Moves ("a2"));
  AttachBlock (g, BlockHash (13), Moves ("c3"));

  EXPECT_EQ (GetState (g), State::AT_TARGET);
  ExpectGameState (BlockHash (12), "a2b1");
}

TEST_F (TargetBlockTests, StopsWhileDetaching)
{
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  AttachBlock (g, BlockHash (12), Moves ("a2"));
  AttachBlock (g, BlockHash (13), Moves ("c3"));

  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (13), "a2b1c3");

  mockXayaServer->SetBestBlock (13, BlockHash (13));
  SetTargetBlock (g, BlockHash (12));

  DetachBlock (g);
  DetachBlock (g);
  DetachBlock (g);

  EXPECT_EQ (GetState (g), State::AT_TARGET);
  ExpectGameState (BlockHash (12), "a2b1");
}

TEST_F (TargetBlockTests, AtTargetWhileSetting)
{
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  AttachBlock (g, BlockHash (12), Moves ("a2"));

  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_EQ (rules.GetLastInstanceState ()["state"].asString (), "up-to-date");

  /* Use the publicly exposed method rather than the internal one to ensure
     that all the logic there (in particular, notifying about the state
     change) is executed.  */
  g.SetTargetBlock (BlockHash (12));

  EXPECT_EQ (GetState (g), State::AT_TARGET);
  EXPECT_EQ (rules.GetLastInstanceState ()["state"].asString (), "at-target");
}

/* ************************************************************************** */

/**
 * A basic coprocessor, which just records all actions seen (initial state,
 * forward, backward) in memory (when those get committed).  It also verifies
 * that the block hashes seen match the test block hashes (and records just
 * the heights for simplicity).
 */
class GameTestCoprocessor : public Coprocessor
{

public:

  /**
   * A recorded (or expected) operation.
   */
  struct Record
  {

    Coprocessor::Op op;
    uint64_t height;

    friend bool
    operator== (const Record& a, const Record& b)
    {
      return a.op == b.op && a.height == b.height;
    }

    friend bool
    operator!= (const Record& a, const Record& b)
    {
      return !(a == b);
    }

  };

private:

  /** The records saved.  */
  std::vector<Record> records;

public:

  class TestBlock;

  GameTestCoprocessor () = default;

  /**
   * Expects that the records match.
   */
  void
  ExpectRecords (const std::vector<Record>& expected) const
  {
    EXPECT_EQ (records, expected);
  }

  std::unique_ptr<Block> ForBlock (const Json::Value& blockData,
                                   Op op) override;

};

class GameTestCoprocessor::TestBlock : public Coprocessor::Block
{

private:

  /** The coprocessor this is attached to, so it can record there.  */
  GameTestCoprocessor& parent;

  /**
   * To mimic a more "realistic" setting similar to how coprocessors will be
   * used in a real game, we let the GameLogic actually call into this
   * instance to initiate saving of the record.  For this a flag is enough,
   * since the actual data is present on the Block already.
   */
  bool write = false;

protected:

  void
  Commit () override
  {
    if (!write)
      {
        LOG (ERROR) << "Commit() called but 'write' on TestBlock is false";
        return;
      }

    if (GetBlockHeight () == GAME_GENESIS_HEIGHT)
      EXPECT_EQ (GetBlockHash (), TestGame::GenesisBlockHash ());
    else
      EXPECT_EQ (GetBlockHash (), BlockHash (GetBlockHeight ()));

    parent.records.push_back ({GetOperation (), GetBlockHeight ()});
  }

public:

  TestBlock (const Json::Value& blockData, const Coprocessor::Op o,
             GameTestCoprocessor& p)
    : Block(blockData, o), parent(p)
  {}

  /**
   * Signals this instance to write a record if Commit() is called.
   */
  void
  WriteRecord ()
  {
    write = true;
  }

};

std::unique_ptr<Coprocessor::Block>
GameTestCoprocessor::ForBlock (const Json::Value& blockData, const Op op)
{
  return std::make_unique<TestBlock> (blockData, op, *this);
}

/**
 * Subclass of TestGame that calls into the coprocessor (if it is available).
 */
class CoprocessorTestGame : public TestGame
{

private:

  /**
   * Gets the coprocessor from the context, and if it is set,
   * calls WriteRecord on it.
   */
  void
  WriteRecord ()
  {
    ASSERT_EQ (
        GetContext ().GetCoprocessor<GameTestCoprocessor::TestBlock> ("foo"),
        nullptr);
    auto* coproc
        = GetContext ().GetCoprocessor<GameTestCoprocessor::TestBlock> ("test");
    if (coproc != nullptr)
      coproc->WriteRecord ();
  }

protected:

  GameStateData
  GetInitialStateInternal (unsigned& height, std::string& hashHex) override
  {
    WriteRecord ();
    return TestGame::GetInitialStateInternal (height, hashHex);
  }

  GameStateData
  ProcessForwardInternal (const GameStateData& oldState,
                          const Json::Value& blockData,
                          UndoData& undoData) override
  {
    WriteRecord ();
    return TestGame::ProcessForwardInternal (oldState, blockData, undoData);
  }

  GameStateData
  ProcessBackwardsInternal (const GameStateData& newState,
                            const Json::Value& blockData,
                            const UndoData& undoData) override
  {
    WriteRecord ();
    return TestGame::ProcessBackwardsInternal (newState, blockData, undoData);
  }

};

class GameCoprocessorTests : public SyncingTests
{

protected:

  /** The coprocessor used.  */
  GameTestCoprocessor coproc;

  /** The modified test game.  */
  CoprocessorTestGame rules;

  GameCoprocessorTests ()
  {
    g.AddCoprocessor ("test", coproc);
    g.SetGameLogic (rules);
    storage.Clear ();
    ReinitialiseState (g);
  }

};

TEST_F (GameCoprocessorTests, CoprocessorCalled)
{
  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  AttachBlock (g, BlockHash (12), Moves ("a2c3"));
  DetachBlock (g);

  coproc.ExpectRecords ({
    {Coprocessor::Op::INITIALISATION, GAME_GENESIS_HEIGHT},
    {Coprocessor::Op::FORWARD, 11},
    {Coprocessor::Op::FORWARD, 12},
    {Coprocessor::Op::BACKWARD, 12},
  });
}

/* ************************************************************************** */

class PendingMoveUpdateTests : public SyncingTests
{

protected:

  TestPendingMoves proc;

  PendingMoveUpdateTests ()
  {
    g.SetPendingMoveProcessor (proc);
    SetMempool ({});
  }

  /**
   * Sets up the mempool that should be returned by the mock server.
   * The txid's are constructed by hashing the given strings.
   */
  void
  SetMempool (const std::vector<std::string>& vals)
  {
    Json::Value mempool(Json::arrayValue);
    for (const auto& v : vals)
      mempool.append (SHA256::Hash (v).ToHex ());

    EXPECT_CALL (*mockXayaServer, getrawmempool ())
        .WillRepeatedly (Return (mempool));
  }

};

TEST_F (PendingMoveUpdateTests, CatchingUp)
{
  EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (12), "reqtoken")));

  mockXayaServer->SetBestBlock (12, BlockHash (12));
  ReinitialiseState (g);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");

  CallBlockAttach (g, "reqtoken",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   Moves ("a0b1"), NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (BlockHash (11), "a0b1");

  CallBlockDetach (g, "reqtoken",
                   TestGame::GenesisBlockHash (), BlockHash (11), 11,
                   NO_SEQ_MISMATCH);
  EXPECT_EQ (GetState (g), State::CATCHING_UP);
  ExpectGameState (TestGame::GenesisBlockHash (), "");

  CallPendingMove (g, Moves ("ax")[0]);

  /* No updates should have been processed at all.  */
  EXPECT_EQ (proc.ToJson (), ParseJson ("{}"));
}

TEST_F (PendingMoveUpdateTests, PendingMoves)
{
  AttachBlock (g, BlockHash (11), Moves (""));

  const auto mv = Moves ("axbyaz");
  for (const auto& mv : Moves ("axbyaz"))
    CallPendingMove (g, mv);

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "state": "",
    "height": 11,
    "a": "z",
    "b": "y"
  })"));
}

TEST_F (PendingMoveUpdateTests, BlockAttach)
{
  SetMempool ({"y"});

  for (const auto& mv : Moves ("axby"))
    CallPendingMove (g, mv);

  AttachBlock (g, BlockHash (11), Moves ("a0b1"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "a0b1");

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "state": "a0b1",
    "height": 11,
    "b": "y"
  })"));
}

TEST_F (PendingMoveUpdateTests, BlockDetach)
{
  AttachBlock (g, BlockHash (11), Moves (""));
  SetMempool ({"x"});

  AttachBlock (g, BlockHash (12), Moves ("ax"));
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (12), "ax");

  DetachBlock (g);
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  ExpectGameState (BlockHash (11), "");

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "state": "",
    "height": 11,
    "a": "x"
  })"));
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
  EXPECT_CALL (*mockXayaServer, game_sendupdates (_, GAME_ID))
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

    EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
        .WillOnce (Return (SendupdatesResponse (BlockHash (12), "reqtoken")));

    EXPECT_CALL (fallibleStorage, BeginTransactionMock ());
    EXPECT_CALL (fallibleStorage, CommitTransactionMock ());
  }

  mockXayaServer->SetBestBlock (12, BlockHash (12));
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
  Json::Value mockHeader(Json::objectValue);
  mockHeader["height"] = GAME_GENESIS_HEIGHT;
  EXPECT_CALL (*mockXayaServer, getblockheader (GAME_GENESIS_HASH))
      .WillRepeatedly (Return (mockHeader));

  EXPECT_CALL (*mockXayaServer, game_sendupdates (GAME_GENESIS_HASH, GAME_ID))
      .WillOnce (Return (SendupdatesResponse (BlockHash (11), "reqtoken")));
  mockXayaServer->SetBestBlock (11, BlockHash (11));

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
  Json::Value mockHeader(Json::objectValue);
  mockHeader["height"] = 11;
  EXPECT_CALL (*mockXayaServer, getblockheader (BlockHash (11).ToHex ()))
      .WillRepeatedly (Return (mockHeader));

  EXPECT_CALL (*mockXayaServer,
               game_sendupdates (BlockHash (11).ToHex (), GAME_ID))
      .WillOnce (Return (SendupdatesResponse (TestGame::GenesisBlockHash (),
                                              "reqtoken")));
  mockXayaServer->SetBestBlock (10, TestGame::GenesisBlockHash ());

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

/**
 * Test fixture for the probe-and-reconnect logic in Game.
 */
class GameProbeAndFixConnectionTests : public SyncingTests
{

protected:

  static constexpr auto MAX_STALENESS = std::chrono::milliseconds (1'000);
  static constexpr auto PING_STALENESS = MAX_STALENESS / 2;

  GameProbeAndFixConnectionTests ()
  {
    FLAGS_xaya_zmq_staleness_ms = MAX_STALENESS.count ();

    AttachBlock (g, BlockHash (11), Moves ("a0b1"));
    EXPECT_EQ (GetState (g), State::UP_TO_DATE);
    mockXayaServer->SetBestBlock (11, BlockHash (11));

    /* We need to be able to start the ZMQ subscriber, even if for the test
       no publisher is connected to it.  */
    const Json::Value notifications = ParseJson (R"(
      [
        {"type": "pubgameblocks", "address": "ipc:///tmp/xayagame_game_tests"}
      ]
    )");
    EXPECT_CALL (*mockXayaServer, getzmqnotifications ())
        .WillRepeatedly (Return (notifications));

    g.DetectZmqEndpoint ();
    GameTestFixture::GetZmq (g).Start ();
  }

  void
  ProbeAndFix ()
  {
    LOG (INFO) << "Probing game connection...";
    GameTestFixture::ProbeAndFixConnection (g);
    /* Sleep some time (short compared to staleness) to make sure any
       potential stop request for ZMQ would be processed by now.  */
    std::this_thread::sleep_for (MAX_STALENESS / 5);
  }

  /**
   * Sets mock expectations for n "ping" calls to game_sendupdates.
   */
  void
  ExpectPings (const unsigned n)
  {
    auto& call =
      EXPECT_CALL (*mockXayaServer,
                   game_sendupdates (TestGame::GenesisBlockHash ().ToHex (),
                                     GAME_ID))
          .Times (n);
    if (n > 0)
      call.WillRepeatedly (Return (Json::Value (Json::objectValue)));
  }

};

TEST_F (GameProbeAndFixConnectionTests, TooEarlyForPing)
{
  ExpectPings (0);
  ProbeAndFix ();
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_TRUE (GameTestFixture::GetZmq (g).IsRunning ());
}

TEST_F (GameProbeAndFixConnectionTests, SendsPing)
{
  ExpectPings (1);
  std::this_thread::sleep_for (3 * MAX_STALENESS / 4);
  ProbeAndFix ();
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_TRUE (GameTestFixture::GetZmq (g).IsRunning ());
}

TEST_F (GameProbeAndFixConnectionTests, DisconnectAndReconnect)
{
  ExpectPings (0);
  EXPECT_CALL (*mockXayaServer, trackedgames ("add", GAME_ID));

  std::this_thread::sleep_for (2 * MAX_STALENESS);
  ProbeAndFix ();
  EXPECT_EQ (GetState (g), State::DISCONNECTED);
  EXPECT_EQ (rules.GetLastInstanceState ()["state"].asString (),
             "disconnected");
  EXPECT_FALSE (GameTestFixture::GetZmq (g).IsRunning ());

  ProbeAndFix ();
  EXPECT_EQ (GetState (g), State::UP_TO_DATE);
  EXPECT_EQ (rules.GetLastInstanceState ()["state"].asString (), "up-to-date");
  EXPECT_TRUE (GameTestFixture::GetZmq (g).IsRunning ());
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
