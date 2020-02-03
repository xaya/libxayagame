// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_TESTUTILS_HPP
#define XAYASHIPS_TESTUTILS_HPP

#include "grid.hpp"
#include "logic.hpp"

#include <xayagame/sqlitestorage.hpp>
#include <xayagame/testutils.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>

#include <json/json.h>

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <string>

namespace ships
{

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

protected:

  xaya::HttpRpcServer<xaya::MockXayaRpcServer> mockXayaServer;

  ShipsLogic game;

  /**
   * Initialises the test case.  This connects the game instance to an
   * in-memory database and sets up the schema on it.
   */
  InMemoryLogicFixture ();

  /**
   * Returns the raw database handle of the test game.
   */
  xaya::SQLiteDatabase& GetDb ();

  /**
   * Returns our board rules.
   */
  const ShipsBoardRules& GetBoardRules () const;

};

} // namespace ships

#endif // XAYASHIPS_TESTUTILS_HPP
