// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gtest/gtest.h>

#include <sstream>
#include <stack>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using xaya::Chain;
using xaya::GameStateData;
using xaya::UndoData;

namespace mover
{

/* ************************************************************************** */

class ParseMoveTests : public testing::Test
{

protected:

  void
  ExpectValid (const std::string& jsonStr, const proto::Direction expectedDir,
               const unsigned expectedSteps) const
  {
    Json::Value move;
    std::istringstream in(jsonStr);
    in >> move;

    proto::Direction dir;
    unsigned steps;
    ASSERT_TRUE (MoverLogic::ParseMove (move, dir, steps));
    EXPECT_EQ (dir, expectedDir);
    EXPECT_EQ (steps, expectedSteps);
  }

  void
  ExpectInvalid (const std::string& jsonStr) const
  {
    Json::Value move;
    std::istringstream in(jsonStr);
    in >> move;

    proto::Direction dir;
    unsigned steps;
    EXPECT_FALSE (MoverLogic::ParseMove (move, dir, steps));
  }

};

namespace
{

TEST_F (ParseMoveTests, ValidMinimalSteps)
{
  ExpectValid (R"(
    {
      "d": "k",
      "n": 1
    }
  )", proto::UP, 1);
}

TEST_F (ParseMoveTests, ValidMaximalSteps)
{
  ExpectValid (R"(
    {
      "n": 1000000,
      "d": "b"
    }
  )", proto::LEFT_DOWN, 1000000);
}

TEST_F (ParseMoveTests, InvalidNoObject)
{
  ExpectInvalid ("[]");
  ExpectInvalid ("\"a\"");
  ExpectInvalid ("42");
}

TEST_F (ParseMoveTests, InvalidWrongKeys)
{
  ExpectInvalid ("{}");
  ExpectInvalid (R"(
    {
      "n": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k"
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "x": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 42,
      "x": 42
    }
  )");
}

TEST_F (ParseMoveTests, InvalidDirection)
{
  ExpectInvalid (R"(
    {
      "d": 42,
      "n": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "kk",
      "n": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "x",
      "n": 42
    }
  )");
}

TEST_F (ParseMoveTests, InvalidSteps)
{
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": "k"
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 0
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 2.5
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": -1
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 1000001
    }
  )");
}

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
      rules.SetGameId ("mv");
      rules.SetChain (chain);

      unsigned height;
      std::string hashHex;
      const GameStateData state = rules.GetInitialState (height, hashHex);

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
    rules.SetGameId ("mv");
    rules.SetChain (Chain::MAIN);

    unsigned height;
    std::string hashHex;
    initialState = rules.GetInitialState (height, hashHex);
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
        currentState = rules.ProcessBackwards (d.newState, d.blockData, d.undo);
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

    d.newState = rules.ProcessForward (currentState, d.blockData, d.undo);
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

} // anonymous namespace
} // namespace mover
