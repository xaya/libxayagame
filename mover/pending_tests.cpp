// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pending.hpp"

#include "proto/mover.pb.h"

#include <google/protobuf/text_format.h>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <sstream>

using google::protobuf::TextFormat;

namespace mover
{

class PendingMoveTests : public testing::Test
{

private:

  PendingMoves proc;

  /**
   * The last set "current confirmed game state" for passing to
   * AddPendingMoveInternal when we call the processor.
   */
  proto::GameState confirmedState;

protected:

  /**
   * Calls clear on the processor.
   */
  void
  Clear ()
  {
    proc.Clear ();
  }

  /**
   * Sets the state to use as current confirmed state based on a text proto.
   */
  void
  SetConfirmedState (const std::string& str)
  {
    CHECK (TextFormat::ParseFromString (str, &confirmedState));
  }

  /**
   * Adds a move given as JSON string to the processor.
   */
  void
  AddMove (const std::string& name, const std::string& mv)
  {
    Json::Value mvData(Json::objectValue);
    mvData["name"] = name;

    std::istringstream in(mv);
    in >> mvData["move"];

    std::string confirmedStateStr;
    CHECK (confirmedState.SerializeToString (&confirmedStateStr));

    proc.AddPendingMoveInternal (confirmedStateStr, mvData);
  }

  /**
   * Expects that the JSON state of the pending moves matches the
   * given string.
   */
  void
  ExpectJsonState (const std::string& str)
  {
    Json::Value expected;
    std::istringstream in(str);
    in >> expected;

    EXPECT_EQ (proc.ToJson (), expected);
  }

};

namespace
{

TEST_F (PendingMoveTests, NewPlayer)
{
  SetConfirmedState ("");
  AddMove ("foo", R"(
    {
      "d": "k",
      "n": 2
    }
  )");

  ExpectJsonState (R"(
    {
      "foo":
        {
          "dir": "up",
          "steps": 2,
          "target": {"x": 0, "y": 2}
        }
    }
  )");
}

TEST_F (PendingMoveTests, ExistingPlayer)
{
  SetConfirmedState (R"(
    players:
      {
        key: "foo"
        value: {x: 42, y: -10, dir: UP, steps_left: 5}
      }
  )");
  AddMove ("foo", R"(
    {
      "d": "h",
      "n": 5
    }
  )");

  ExpectJsonState (R"(
    {
      "foo":
        {
          "dir": "left",
          "steps": 5,
          "target": {"x": 37, "y": -10}
        }
    }
  )");
}

TEST_F (PendingMoveTests, MultipleMoves)
{
  SetConfirmedState ("");
  AddMove ("foo", R"(
    {
      "d": "h",
      "n": 5
    }
  )");
  AddMove ("foo", R"(
    {
      "d": "j",
      "n": 2
    }
  )");

  ExpectJsonState (R"(
    {
      "foo":
        {
          "dir": "down",
          "steps": 2,
          "target": {"x": 0, "y": -2}
        }
    }
  )");
}

TEST_F (PendingMoveTests, MultiplePlayers)
{
  SetConfirmedState ("");
  AddMove ("foo", R"(
    {
      "d": "h",
      "n": 5
    }
  )");
  AddMove ("bar", R"(
    {
      "d": "j",
      "n": 2
    }
  )");

  ExpectJsonState (R"(
    {
      "foo":
        {
          "dir": "left",
          "steps": 5,
          "target": {"x": -5, "y": 0}
        },
      "bar":
        {
          "dir": "down",
          "steps": 2,
          "target": {"x": 0, "y": -2}
        }
    }
  )");
}

TEST_F (PendingMoveTests, InvalidMove)
{
  SetConfirmedState ("");
  AddMove ("foo", R"(
    {
      "d": "x",
      "n": 5
    }
  )");

  ExpectJsonState ("{}");
}

} // anonymous namespace
} // namespace mover
