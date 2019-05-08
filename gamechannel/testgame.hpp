// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_TESTGAME_HPP
#define GAMECHANNEL_TESTGAME_HPP

#include "boardrules.hpp"
#include "channelgame.hpp"

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
 * The current state is a number, encoded simply in a string.  The current
 * turn is for player (number % 2).  When the number is 100 or above, then
 * the game is finished.  A move is simply another, strictly positive number
 * encoded as a string, which gets added to the current "state number".
 */
class AdditionRules : public BoardRules
{

public:

  bool CompareStates (const ChannelMetadata& meta,
                      const BoardState& a, const BoardState& b) const override;

  int WhoseTurn (const ChannelMetadata& meta,
                 const BoardState& a) const override;

  bool ApplyMove (const ChannelMetadata& meta,
                  const BoardState& oldState, const BoardMove& mv,
                  BoardState& newState) const override;

};

/**
 * Subclass of ChannelGame that implements a trivial game only as much as
 * necessary for unit tests of the game-channel framework.
 */
class TestGame : public ChannelGame
{

private:

  AdditionRules rules;

protected:

  void SetupSchema (sqlite3* db) override;
  void GetInitialStateBlock (unsigned& height,
                             std::string& hashHex) const override;
  void InitialiseState (sqlite3* db) override;
  void UpdateState (sqlite3* db, const Json::Value& blockData) override;
  Json::Value GetStateAsJson (sqlite3* db) override;

  const BoardRules& GetBoardRules () const override;

  friend class TestGameFixture;

};

/**
 * Test fixture that constructs a TestGame instance with an in-memory database
 * and exposes that to the test itself.
 */
class TestGameFixture : public testing::Test
{

protected:

  /** TestGame instance that is used in the test.  */
  TestGame game;

  /**
   * Initialises the test case.  This connects the game instance to an
   * in-memory database and sets up the schema on it.
   */
  TestGameFixture ();

  /**
   * Returns the raw database handle of the test game.
   */
  sqlite3* GetDb ();

};

} // namespace xaya

#endif // GAMECHANNEL_TESTGAME_HPP
