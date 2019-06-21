// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_TESTUTILS_HPP
#define XAYASHIPS_TESTUTILS_HPP

#include "grid.hpp"
#include "logic.hpp"

#include <xayagame/testutils.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>

#include <json/json.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <string>

namespace ships
{

/**
 * Parses a ship position given as string and returns it.  The string must be
 * 64 characters long (and may be split into 8 lines in code), consisting only
 * of the characters "." for zeros and "x" for ones.
 */
Grid GridFromString (const std::string& str);

/**
 * Parses a string into JSON.
 */
Json::Value ParseJson (const std::string& str);

/**
 * Test fixture that creates a ShipsLogic instance with an in-memory database
 * for testing of the on-chain GSP code.  It also includes a mock RPC server
 * for signature verification.
 */
class InMemoryLogicFixture : public testing::Test
{

private:

  jsonrpc::HttpServer httpServer;
  jsonrpc::HttpClient httpClient;

protected:

  xaya::MockXayaRpcServer mockXayaServer;
  XayaRpcClient rpcClient;

  ShipsLogic game;

  /**
   * Initialises the test case.  This connects the game instance to an
   * in-memory database and sets up the schema on it.
   */
  InMemoryLogicFixture ();

  /**
   * The destructor shuts down the mock Xaya server.
   */
  ~InMemoryLogicFixture ();

  /**
   * Returns the raw database handle of the test game.
   */
  sqlite3* GetDb ();

};

} // namespace ships

#endif // XAYASHIPS_TESTUTILS_HPP
