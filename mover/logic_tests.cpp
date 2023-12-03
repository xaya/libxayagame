// Copyright (C) 2018-2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gtest/gtest.h>

#include <sstream>
#include <stack>

namespace mover
{
namespace
{

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using xaya::Chain;
using xaya::GameStateData;
using xaya::UndoData;

/* ************************************************************************** */

TEST (InitialStateTests, IsEmpty)
{
  /* We do not want to verify the blocks/heights for the initial state, as
     that would just be duplicating the magic values here.  But we verify that
     the game state is empty.  */
  const proto::GameState expectedState;

  for (const auto chain : {Chain::MAIN, Chain::TEST, Chain::REGTEST})
    {
      MoverLogic rules;
      rules.InitialiseGameContext (chain, "mv", nullptr);

      unsigned height;
      std::string hashHex;
      const GameStateData state
          = rules.GetInitialState (height, hashHex, nullptr);

      proto::GameState actualState;
      ASSERT_TRUE (actualState.ParseFromString (state));
      EXPECT_TRUE (MessageDifferencer::Equals (actualState, expectedState));
    }
}

/* ************************************************************************** */

/**
 * Test fixture for verifying the processing of game logic.  It is given a
 * series of moves and the expected resulting game states.  It also records
 * all those states and the corresponding undo data, and then verifies that
 * rolling backwards all states works as well.
 */
class StateProcessingTests : public testing::Test
{

private:

  MoverLogic rules;

  struct ForBlockData
  {
    Json::Value blockData;
    GameStateData newState;
    UndoData undo;
  };

  std::stack<ForBlockData> blocks;

  GameStateData initialState;
  GameStateData currentState;

  /**
   * Verifies that two game states given in the encoded string format are equal.
   * This converts them to protobuf and compares those, to avoid issues due
   * to non-deterministic proto serialisation.
   */
  void
  VerifyStatesEqual (const GameStateData& s1, const GameStateData& s2)
  {
    proto::GameState pb1;
    ASSERT_TRUE (pb1.ParseFromString (s1));

    proto::GameState pb2;
    ASSERT_TRUE (pb2.ParseFromString (s2));

    EXPECT_TRUE (MessageDifferencer::Equals (pb1, pb2))
        << "State 1:\n" << pb1.DebugString ()
        << "\nState 2:\n" << pb2.DebugString ();
  }

protected:

  StateProcessingTests ()
  {
    rules.InitialiseGameContext (Chain::MAIN, "mv", nullptr);

    unsigned height;
    std::string hashHex;
    initialState = rules.GetInitialState (height, hashHex, nullptr);
    currentState = initialState;
  }

  ~StateProcessingTests ()
  {
    /* Roll back all performed forward steps and verify that the undoing
       works as expected.  */
    while (!blocks.empty ())
      {
        const auto& d = blocks.top ();
        VerifyStatesEqual (currentState, d.newState);
        currentState = rules.ProcessBackwards (d.newState, d.blockData,
                                               d.undo, nullptr);
        blocks.pop ();
      }
    VerifyStatesEqual (currentState, initialState);
  }

  /**
   * Processes forward one block:  The move data is given, and the result
   * is compared to the expected data.  State and undo data are recorded
   * internally.
   *
   * Note that move data here is a text-format JSON dictionary mapping names
   * to their moves directly; not the "moves" array from the ZMQ notification
   * format.  This is all that is used by the Mover game, and thus makes it
   * simpler to specify those moves in test cases.
   */
  void
  VerifyForwardStep (const std::string& movesJson,
                     const std::string& expectedStateText)
  {
    ForBlockData d;

    Json::Value moves;
    std::istringstream in(movesJson);
    in >> moves;

    Json::Value moveArray(Json::arrayValue);
    for (const auto& nm : moves.getMemberNames ())
      {
        Json::Value elem(Json::objectValue);
        elem["name"] = nm;
        elem["move"] = moves[nm];
        moveArray.append (elem);
      }

    Json::Value blk(Json::objectValue);
    blk["rngseed"]
        = "0000000000000000000000000000000000000000000000000000000000000001";

    d.blockData = Json::Value (Json::objectValue);
    d.blockData["block"] = blk;
    d.blockData["moves"] = moveArray;

    proto::GameState expectedStatePb;
    ASSERT_TRUE (TextFormat::ParseFromString (expectedStateText,
                                              &expectedStatePb));
    GameStateData expectedState;
    ASSERT_TRUE (expectedStatePb.SerializeToString (&expectedState));

    d.newState = rules.ProcessForward (currentState, d.blockData,
                                       d.undo, nullptr);
    VerifyStatesEqual (d.newState, expectedState);

    currentState = d.newState;
    blocks.push (std::move (d));
  }

};

TEST_F (StateProcessingTests, EmptyMoves)
{
  for (unsigned i = 0; i < 10; ++i)
    VerifyForwardStep ("{}", "");
}

TEST_F (StateProcessingTests, InvalidMoveIgnored)
{
  VerifyForwardStep (R"(
    {
      "a": {"this is": "not a valid move"}
    }
  )", "");
}

TEST_F (StateProcessingTests, MovingAround)
{
  VerifyForwardStep (R"(
    {
      "a": {"d": "k", "n": 2},
      "b": {"d": "l", "n": 1}
    }
  )", R"(
    players: {key: "a", value: {x: 0, y: 1, dir: UP, steps_left: 1}}
    players: {key: "b", value: {x: 1, y: 0, dir: NONE, steps_left: 0}}
  )");

  VerifyForwardStep (R"(
    {
      "a": {"d": "j", "n": 1},
      "c": {"d": "y", "n": 2}
    }
  )", R"(
    players: {key: "a", value: {x: 0, y: 0, dir: NONE, steps_left: 0}}
    players: {key: "b", value: {x: 1, y: 0, dir: NONE, steps_left: 0}}
    players: {key: "c", value: {x: -1, y: 1, dir: LEFT_UP, steps_left: 1}}
  )");

  VerifyForwardStep ("{}", R"(
    players: {key: "a", value: {x: 0, y: 0, dir: NONE, steps_left: 0}}
    players: {key: "b", value: {x: 1, y: 0, dir: NONE, steps_left: 0}}
    players: {key: "c", value: {x: -2, y: 2, dir: NONE, steps_left: 0}}
  )");
}

/* ************************************************************************** */

TEST (GameStateToJsonTests, Works)
{
  MoverLogic rules;

  proto::GameState statePb;
  ASSERT_TRUE (TextFormat::ParseFromString (R"(
    players: {key: "a", value: {x: 5, y: -2, dir: NONE}}
    players: {key: "b", value: {x: 0, y: 0, dir: UP, steps_left: 42}}
  )", &statePb));

  GameStateData state;
  ASSERT_TRUE (statePb.SerializeToString (&state));
  const Json::Value json = rules.GameStateToJson (state);

  Json::Value expectedJson;
  std::istringstream in(R"(
    {
      "players":
        {
          "a": {"x": 5, "y": -2},
          "b": {"x": 0, "y": 0, "dir": "up", "steps": 42}
        }
    }
  )");
  in >> expectedJson;

  EXPECT_TRUE (json == expectedJson)
      << "Actual:\n" << json << "\nExpected:\n" << expectedJson;
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace mover
