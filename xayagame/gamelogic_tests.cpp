// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamelogic.hpp"

#include "storage.hpp"

#include "testutils.hpp"

#include <json/json.h>

#include <gtest/gtest.h>

#include <glog/logging.h>

#include <stack>

namespace xaya
{
namespace
{

constexpr const char GAME_ID[] = "test game";

/**
 * Fixture class with a simulated blockchain for verifying basic working
 * of GameLogic instances without the need to have a real Game instance.
 */
template <typename G>
  class GameLogicFixture : public testing::Test
{

protected:

  G game;

  /** The current game state in the simulated blockchain.  */
  GameStateData state;
  /** The stack of block data that has been attached.  */
  std::stack<Json::Value> blockStack;
  /** The stack of undo data for the simulated blockchain.  */
  std::stack<UndoData> undoStack;

  GameLogicFixture ()
  {
    game.SetGameId (GAME_ID);

    unsigned dummyHeight;
    std::string dummyHashHex;
    state = game.GetInitialState (dummyHeight, dummyHashHex);
  }

  /**
   * Processes the state forward using game and the simulated blockchain.
   */
  void
  AttachBlock (const Json::Value& moves)
  {
    Json::Value blk(Json::objectValue);
    blk["rngseed"] = BlockHash (blockStack.size ()).ToHex ();

    Json::Value blockData(Json::objectValue);
    blockData["block"] = blk;
    blockData["moves"] = moves;

    blockStack.push (blockData);

    UndoData undo;
    state = game.ProcessForward (state, blockData, undo);
    undoStack.push (undo);
  }

  /**
   * Processes the state backwards using game and our simulated blockchain.
   */
  void
  DetachBlock ()
  {
    state = game.ProcessBackwards (state, blockStack.top (), undoStack.top ());

    undoStack.pop ();
    blockStack.pop ();
  }

  /**
   * Constructs a moves array that has no actual data.
   */
  static Json::Value
  NoMove ()
  {
    return Json::Value (Json::arrayValue);
  }

};

/* ************************************************************************** */

/**
 * A very simple game implemented using CachingGame:  The state is just a string
 * that can be changed.  The move is the new value, which replaces the old one.
 */
class ReplacingGame : public CachingGame
{

protected:

  GameStateData
  UpdateState (const GameStateData& oldState,
               const Json::Value& blockData) override
  {
    if (blockData["moves"].empty ())
      return oldState;
    return blockData["moves"][0]["move"].asString ();
  }

  GameStateData
  GetInitialStateInternal (unsigned& height, std::string& hashHex) override
  {
    return "";
  }

};

class CachingGameTests : public GameLogicFixture<ReplacingGame>
{

protected:

  /**
   * Constructs a move array that sets the given value.
   */
  static Json::Value
  Move (const std::string& value)
  {
    Json::Value move(Json::objectValue);
    move["move"] = value;

    Json::Value moves(Json::arrayValue);
    moves.append (move);

    return moves;
  }

};

TEST_F (CachingGameTests, Works)
{
  AttachBlock (Move ("foo"));
  EXPECT_EQ (state, "foo");
  AttachBlock (Move ("bar"));
  EXPECT_EQ (state, "bar");

  DetachBlock ();
  EXPECT_EQ (state, "foo");

  AttachBlock (NoMove ());
  EXPECT_EQ (state, "foo");
  AttachBlock (Move ("baz"));
  EXPECT_EQ (state, "baz");

  DetachBlock ();
  EXPECT_EQ (state, "foo");
  DetachBlock ();
  EXPECT_EQ (state, "foo");
  DetachBlock ();
  EXPECT_TRUE (blockStack.empty ());
  EXPECT_EQ (state, "");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
