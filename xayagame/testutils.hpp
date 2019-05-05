// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_TESTUTILS_HPP
#define XAYAGAME_TESTUTILS_HPP

/* Shared utility functions for unit tests of xayagame.  */

#include "game.hpp"
#include "storage.hpp"

#include "rpc-stubs/xayarpcserverstub.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>
#include <jsonrpccpp/server.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <mutex>
#include <vector>
#include <string>

namespace xaya
{

/**
 * Returns a uint256 based on the given number, to be used as block hashes
 * in tests.
 */
uint256 BlockHash (unsigned num);

/**
 * Sleep for "some time" to give other threads time to run and simulate
 * some delay in a real application.  This is a hack, but it works well for
 * some test situations we need.
 */
void SleepSome ();

/**
 * Parses a given string as JSON and returns the JSON object.
 */
Json::Value ParseJson (const std::string& str);

/**
 * Mock server for Xaya Core's JSON-RPC interface.
 */
class MockXayaRpcServer : public XayaRpcServerStub
{

public:

  /** The listening port for the mock server in tests.  */
  static constexpr int HTTP_PORT = 32100;

  /** The listening address of the mock server.  */
  static constexpr const char* HTTP_URL = "http://localhost:32100";

  /**
   * Constructs the server at the given connector.  It does not start
   * listening yet.
   *
   * This also sets the mock expectations to no calls at all, so that
   * tests can explicitly specify the calls they want.
   */
  explicit MockXayaRpcServer (jsonrpc::AbstractServerConnector& conn);

  MockXayaRpcServer () = delete;
  MockXayaRpcServer (const MockXayaRpcServer&) = delete;
  void operator= (const MockXayaRpcServer&) = delete;

  MOCK_METHOD0 (getzmqnotifications, Json::Value ());
  MOCK_METHOD2 (trackedgames, void (const std::string& command,
                                    const std::string& gameid));
  MOCK_METHOD0 (getnetworkinfo, Json::Value ());
  MOCK_METHOD0 (getblockchaininfo, Json::Value ());
  MOCK_METHOD1 (getblockhash, std::string (int height));
  MOCK_METHOD1 (getblockheader, Json::Value (const std::string& hash));
  MOCK_METHOD2 (game_sendupdates, Json::Value (const std::string& fromblock,
                                               const std::string& gameid));
  MOCK_METHOD3 (verifymessage, Json::Value (const std::string& address,
                                            const std::string& message,
                                            const std::string& signature));

};

/**
 * Memory storage instance that has mocks for verifying the transaction
 * methods that are called.
 */
class TxMockedMemoryStorage : public MemoryStorage
{

public:

  /* Mock the transaction methods.  We still want to call through to the base
     class' method, thus we mock other methods and call them together with
     the base class method from the overridden real methods.  */
  MOCK_METHOD0 (BeginTransactionMock, void ());
  MOCK_METHOD0 (CommitTransactionMock, void ());
  MOCK_METHOD0 (RollbackTransactionMock, void ());

  void
  BeginTransaction () override
  {
    MemoryStorage::BeginTransaction ();
    BeginTransactionMock ();
  }

  void
  CommitTransaction () override
  {
    MemoryStorage::CommitTransaction ();
    CommitTransactionMock ();
  }

  void
  RollbackTransaction () override
  {
    MemoryStorage::RollbackTransaction ();
    RollbackTransactionMock ();
  }

};


/**
 * Simple test fixture for tests that interact with a Game instance.  It is
 * declared friend of Game and makes some of the internal methods available
 * to the tests.
 */
class GameTestFixture : public testing::Test
{

private:

  /** Game ID to send to BlockAttach / BlockDetach.  */
  const std::string gameId;

protected:

  using State = Game::State;

  explicit GameTestFixture (const std::string& id)
    : gameId(id)
  {}

  static void
  SetUpTestCase ()
  {
    /* Use JSON-RPC V2 by the RPC client in Game.  It seems that V1 to V1
       does not work with jsonrpccpp for some reason.  */
    Game::rpcClientVersion = jsonrpc::JSONRPC_CLIENT_V2;
  }

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
  TrackGame (Game& g)
  {
    g.TrackGame ();
  }

  static void
  UntrackGame (Game& g)
  {
    g.UntrackGame ();
  }

  static void
  ReinitialiseState (Game& g)
  {
    std::lock_guard<std::mutex> lock(g.mut);
    g.ReinitialiseState ();
  }

  static void
  ForceState (Game& g, const State s)
  {
    std::lock_guard<std::mutex> lock(g.mut);
    g.state = s;
  }

  /**
   * Calls BlockAttach on the given game instance.  The function takes care
   * of setting up the blockData JSON object correctly based on the building
   * blocks given here.
   */
  void CallBlockAttach (Game& g, const std::string& reqToken,
                        const uint256& parentHash, const uint256& blockHash,
                        unsigned height,
                        const Json::Value& moves, const bool seqMismatch) const;

  /**
   * Calls BlockDetach on the given game instance, setting up the blockData
   * object correctly.
   */
  void CallBlockDetach (Game& g, const std::string& reqToken,
                        const uint256& parentHash, const uint256& blockHash,
                        unsigned height,
                        const Json::Value& moves, const bool seqMismatch) const;

};

/**
 * An extension to the GameTestFixture that keeps track of its own "blockchain"
 * in a stack of block hashes and associated move objects.  This can be
 * used to test basic situations of consistent attaches/detaches more easily
 * than with CallBlockAttach and CallBlockDetach.
 */
class GameTestWithBlockchain : public GameTestFixture
{

private:

  /** Stack of attached block hashes.  */
  std::vector<uint256> blockHashes;
  /**
   * Stack of corresponding move objects (the bottom-most entry in
   * blockHashes was set by SetStartingBlock and doesn't have any moves
   * associated to it).
   */
  std::vector<Json::Value> moveStack;

public:

  using GameTestFixture::GameTestFixture;

  /**
   * Resets the "blockchain" to have the given starting block (need not be
   * the real genesis block, it is just the block from where the next attach
   * will be done).
   */
  void SetStartingBlock (const uint256& hash);

  /**
   * Attaches the given next block on top of the current blockchain stack.
   * This calls BlockAttach on the game instance with no request token (empty
   * string) and without sequence mismatch.
   */
  void AttachBlock (Game& g, const uint256& hash, const Json::Value& moves);

  /**
   * Detaches the current top block from the simulated blockchain.
   */
  void DetachBlock (Game& g);

};

} // namespace xaya

#endif // XAYAGAME_TESTUTILS_HPP
