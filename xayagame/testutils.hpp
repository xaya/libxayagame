// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_TESTUTILS_HPP
#define XAYAGAME_TESTUTILS_HPP

/* Shared utility functions for unit tests of xayagame.  */

#include "game.hpp"
#include "storage.hpp"

#include "rpc-stubs/xayarpcclient.h"
#include "rpc-stubs/xayarpcserverstub.h"
#include "rpc-stubs/xayawalletrpcclient.h"
#include "rpc-stubs/xayawalletrpcserverstub.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
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

  /** The type of the corresponding RPC client.  */
  using RpcClient = XayaRpcClient;

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

  MOCK_METHOD0 (getrawmempool, Json::Value ());
  MOCK_METHOD0 (name_pending, Json::Value ());

};

/**
 * Mock server for Xaya Core's wallet JSON-RPC interface.
 */
class MockXayaWalletRpcServer : public XayaWalletRpcServerStub
{

public:

  /** The listening port for the mock server in tests.  */
  static constexpr int HTTP_PORT = 32101;

  /** The listening address of the mock server.  */
  static constexpr const char* HTTP_URL = "http://localhost:32101";

  /** The type of the corresponding RPC client.  */
  using RpcClient = XayaWalletRpcClient;

  /**
   * Constructs the server at the given connector.  It does not start
   * listening yet.
   *
   * This also sets the mock expectations to no calls at all, so that
   * tests can explicitly specify the calls they want.
   */
  explicit MockXayaWalletRpcServer (jsonrpc::AbstractServerConnector& conn);

  MockXayaWalletRpcServer () = delete;
  MockXayaWalletRpcServer (const MockXayaWalletRpcServer&) = delete;
  void operator= (const MockXayaWalletRpcServer&) = delete;

  MOCK_METHOD1 (getaddressinfo, Json::Value (const std::string& addr));
  MOCK_METHOD2 (signmessage, std::string (const std::string& addr,
                                          const std::string& msg));
  MOCK_METHOD2 (name_update, std::string (const std::string& name,
                                          const std::string& value));

};

/**
 * An RPC client together with its HttpClient connector.
 */
template <typename RpcClient>
  class HttpRpcClient
{

private:

  jsonrpc::HttpClient httpClient;
  RpcClient rpcClient;

public:

  explicit HttpRpcClient (const std::string& url)
    : httpClient(url),
      rpcClient(httpClient)
  {}

  /**
   * Returns the underlying RPC client.
   */
  RpcClient&
  operator* ()
  {
    return rpcClient;
  }

  /**
   * Returns the underlying client connector.
   */
  jsonrpc::HttpClient&
  GetConnector ()
  {
    return httpClient;
  }

};

/**
 * A mock RPC server together with the HttpServer and HttpClient for it.
 * This is what we usually need for a mock server, so it makes it more
 * convenient to have all together.
 */
template <typename MockServer>
  class HttpRpcServer
{

private:

  jsonrpc::HttpServer httpServer;
  MockServer srv;

  HttpRpcClient<typename MockServer::RpcClient> rpcClient;

public:

  template <typename... Args>
    explicit HttpRpcServer (Args&... args)
    : httpServer(MockServer::HTTP_PORT),
      srv(httpServer, args...),
      rpcClient(MockServer::HTTP_URL)
  {
    srv.StartListening ();
  }

  ~HttpRpcServer ()
  {
    srv.StopListening ();
  }

  /**
   * Returns the mock server object, e.g. to set expectations.
   */
  MockServer&
  operator* ()
  {
    return srv;
  }

  MockServer*
  operator-> ()
  {
    return &srv;
  }

  /**
   * Returns the RPC client connection.
   */
  typename MockServer::RpcClient&
  GetClient ()
  {
    return *rpcClient;
  }

  /**
   * Returns the HTTP client connector.
   */
  jsonrpc::HttpClient&
  GetClientConnector ()
  {
    return rpcClient.GetConnector ();
  }

  /**
   * Creates a fresh HTTP client connection (which can be used e.g. from
   * a different thread) and returns it.
   */
  std::unique_ptr<HttpRpcClient<typename MockServer::RpcClient>>
  CreateClient ()
  {
    return std::make_unique<HttpRpcClient<typename MockServer::RpcClient>> (
        MockServer::HTTP_URL);
  }

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
    return g.zmq.addrBlocks;
  }

  static std::string
  GetZmqEndpointPending (const Game& g)
  {
    return g.zmq.addrPending;
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
   * Calls BlockAttach on the given Game instance.  The function takes care
   * of setting up the blockData JSON object correctly based on the building
   * blocks given here.
   */
  void CallBlockAttach (Game& g, const std::string& reqToken,
                        const uint256& parentHash, const uint256& blockHash,
                        unsigned height,
                        const Json::Value& moves, const bool seqMismatch) const;

  /**
   * Calls BlockDetach on the given Game instance, setting up the blockData
   * object correctly.
   */
  void CallBlockDetach (Game& g, const std::string& reqToken,
                        const uint256& parentHash, const uint256& blockHash,
                        unsigned height,
                        const Json::Value& moves, const bool seqMismatch) const;

  /**
   * Calls PendingMove on the given Game instance.
   */
  void CallPendingMove (Game& g, const Json::Value& data) const;

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
