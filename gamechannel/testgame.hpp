// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_TESTGAME_HPP
#define GAMECHANNEL_TESTGAME_HPP

#include "boardrules.hpp"
#include "channelgame.hpp"

#include "xayagame/testutils.hpp"

#include <xayagame/rpc-stubs/xayarpcclient.h>

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <json/json.h>

#include <string>

namespace xaya
{

class TestGameFixture;

/**
 * Board rules for a trivial example game used in unit tests.  The game goes
 * like this:
 *
 * The current state is a pair of numbers, encoded simply in a string.  Those
 * numbers are a "current number" and the turn count.  The current
 * turn is for player (number % 2).  When the number is 100 or above, then
 * the game is finished.  A move is simply another, strictly positive number
 * encoded as a string, which gets added to the current "state number".
 * The turn count is simply incremented on each turn made.
 */
class AdditionRules : public BoardRules
{

public:

  bool CompareStates (const proto::ChannelMetadata& meta,
                      const BoardState& a, const BoardState& b) const override;

  int WhoseTurn (const proto::ChannelMetadata& meta,
                 const BoardState& state) const override;

  unsigned TurnCount (const proto::ChannelMetadata& meta,
                      const BoardState& state) const override;

  bool ApplyMove (const proto::ChannelMetadata& meta,
                  const BoardState& oldState, const BoardMove& mv,
                  BoardState& newState) const override;

};

/**
 * Subclass of ChannelGame that implements a trivial game only as much as
 * necessary for unit tests of the game-channel framework.
 */
class TestGame : public ChannelGame
{

protected:

  void SetupSchema (sqlite3* db) override;
  void GetInitialStateBlock (unsigned& height,
                             std::string& hashHex) const override;
  void InitialiseState (sqlite3* db) override;
  void UpdateState (sqlite3* db, const Json::Value& blockData) override;
  Json::Value GetStateAsJson (sqlite3* db) override;

  const BoardRules& GetBoardRules () const override;

  friend class TestGameFixture;

public:

  AdditionRules rules;

  using ChannelGame::ProcessDispute;
  using ChannelGame::ProcessResolution;

};

/**
 * Test fixture that constructs a TestGame instance with an in-memory database
 * and exposes that to the test itself.  It also runs a mock Xaya Core server
 * for use together with signature verification.
 */
class TestGameFixture : public testing::Test
{

private:

  jsonrpc::HttpServer httpServer;
  jsonrpc::HttpClient httpClient;

protected:

  MockXayaRpcServer mockXayaServer;
  XayaRpcClient rpcClient;

  TestGame game;

  /**
   * Initialises the test case.  This connects the game instance to an
   * in-memory database and sets up the schema on it.
   */
  TestGameFixture ();

  /**
   * The destructor shuts down the mock Xaya server.
   */
  ~TestGameFixture ();

  /**
   * Returns the raw database handle of the test game.
   */
  sqlite3* GetDb ();

  /**
   * Sets up the mock server to validate *any* message with the given
   * signature as belonging to the given address.  sgn is in raw binary
   * (will be base64-encoded for the RPC).
   */
  void ValidSignature (const std::string& sgn, const std::string& addr);

  /**
   * Expects exactly one call to verifymessage with the given message
   * and signature (both as binary, they will be hashed / base64-encoded).
   * Returns a valid response for the given address.
   */
  void ExpectSignature (const std::string& msg, const std::string& sgn,
                        const std::string& addr);

};

} // namespace xaya

#endif // GAMECHANNEL_TESTGAME_HPP
