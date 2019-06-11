// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "board.hpp"

#include "proto/boardstate.pb.h"

#include <gamechannel/proto/metadata.pb.h>
#include <xayautil/hash.hpp>
#include <xayautil/uint256.hpp>

#include <google/protobuf/text_format.h>

#include <gtest/gtest.h>

#include <memory>

using google::protobuf::TextFormat;

namespace ships
{

/* ************************************************************************** */

class BoardTests : public testing::Test
{

protected:

  using Phase = ShipsBoardState::Phase;

  const xaya::uint256 channelId = xaya::SHA256::Hash ("foo");

  /**
   * The metadata used for testing.  This is set to a standard two-player
   * list by default, but may be modified by tests if they want to check what
   * happens in other situations (e.g. only one player in the channel yet).
   */
  xaya::proto::ChannelMetadata meta;

  ShipsBoardRules rules;

  BoardTests ()
  {
    auto* p = meta.add_participants ();
    p->set_name ("alice");
    p->set_address ("addr 1");

    p = meta.add_participants ();
    p->set_name ("bob");
    p->set_address ("addr 2");
  }

  /**
   * Parses a BoardState given as text string into a ShipsBoardState
   * instance.
   */
  std::unique_ptr<ShipsBoardState>
  ParseState (const std::string& str, const bool allowInvalid = false)
  {
    proto::BoardState state;
    CHECK (TextFormat::ParseFromString (str, &state));

    std::string serialised;
    CHECK (state.SerializeToString (&serialised));

    auto res = rules.ParseState (channelId, meta, serialised);
    if (allowInvalid && res == nullptr)
      return nullptr;
    CHECK (res != nullptr);

    auto* ptr = dynamic_cast<ShipsBoardState*> (res.get ());
    CHECK (ptr != nullptr);

    res.release ();
    return std::unique_ptr<ShipsBoardState> (ptr);
  }

  /**
   * Exposes ShipsBoardState::GetPhase to subtests.
   */
  Phase
  GetPhase (const ShipsBoardState& s)
  {
    return s.GetPhase ();
  }

};

namespace
{

/* ************************************************************************** */

class IsValidTests : public BoardTests
{

protected:

  void
  ExpectValid (const std::string& str)
  {
    LOG (INFO) << "Expecting state to be valid: " << str;

    auto p = ParseState (str, true);
    ASSERT_NE (p, nullptr);
    EXPECT_TRUE (p->IsValid ());
  }

  void
  ExpectInvalid (const std::string& str)
  {
    LOG (INFO) << "Expecting state to be invalid: " << str;
    EXPECT_EQ (ParseState (str, true), nullptr);
  }

};

TEST_F (IsValidTests, MalformedData)
{
  EXPECT_EQ (rules.ParseState (channelId, meta, "invalid"), nullptr);
}

TEST_F (IsValidTests, InvalidPhase)
{
  ExpectInvalid (R"(
    position_hashes: "foo"
    position_hashes: "bar"
    position_hashes: "baz"
  )");

  ExpectInvalid (R"(
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: {}
  )");

  ExpectInvalid (R"(
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: {}
    known_ships: {}
    positions: 10
  )");
}

TEST_F (IsValidTests, TurnWhenFinished)
{
  ExpectValid ("winner_statement: {}");
  ExpectInvalid (R"(
    turn: 0
    winner_statement: {}
  )");
}

TEST_F (IsValidTests, MissingTurnWhenNotFinished)
{
  ExpectInvalid ("winner: 0");
}

TEST_F (IsValidTests, TurnOutOfBounds)
{
  ExpectInvalid (R"(
    turn: 2
    winner: 0
  )");
}

TEST_F (IsValidTests, TurnForFirstCommitReveal)
{
  ExpectValid ("turn: 0");
  ExpectInvalid ("turn: 1");

  ExpectValid (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
  )");
  ExpectInvalid (R"(
    turn: 1
    position_hashes: "a"
    position_hashes: "b"
  )");
}

TEST_F (IsValidTests, TurnForSecondCommit)
{
  ExpectValid (R"(
    turn: 1
    position_hashes: "foo"
  )");
  ExpectInvalid (R"(
    turn: 0
    position_hashes: "foo"
  )");
}

TEST_F (IsValidTests, TurnForRevealPosition)
{
  ExpectValid (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
    positions: 0
    positions: 10
  )");
  ExpectValid (R"(
    turn: 1
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
    positions: 10
    positions: 0
  )");

  ExpectInvalid (R"(
    turn: 1
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
    positions: 0
    positions: 10
  )");
  ExpectInvalid (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
    positions: 10
    positions: 0
  )");
}

TEST_F (IsValidTests, TurnForWinnerDetermined)
{
  ExpectValid (R"(
    turn: 0
    winner: 1
  )");
  ExpectValid (R"(
    turn: 1
    winner: 0
  )");

  ExpectInvalid (R"(
    turn: 0
    winner: 0
  )");
  ExpectInvalid (R"(
    turn: 1
    winner: 1
  )");
}

/* ************************************************************************** */

using GetPhaseTests = BoardTests;

TEST_F (GetPhaseTests, PositionCommitments)
{
  EXPECT_EQ (GetPhase (*ParseState ("turn: 0")), Phase::FIRST_COMMITMENT);

  EXPECT_EQ (GetPhase (*ParseState (R"(
    turn: 1
    position_hashes: "foo"
  )")), Phase::SECOND_COMMITMENT);
}

TEST_F (GetPhaseTests, RevealSeed)
{
  EXPECT_EQ (GetPhase (*ParseState (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
  )")), Phase::FIRST_REVEAL_SEED);
}

TEST_F (GetPhaseTests, ShotAndAnswer)
{
  EXPECT_EQ (GetPhase (*ParseState (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
  )")), Phase::SHOOT);

  EXPECT_EQ (GetPhase (*ParseState (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
    current_shot: 42
  )")), Phase::ANSWER);
}

TEST_F (GetPhaseTests, RevealPosition)
{
  EXPECT_EQ (GetPhase (*ParseState (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
    positions: 0
    positions: 10
  )")), Phase::SECOND_REVEAL_POSITION);
}

TEST_F (GetPhaseTests, EndOfGame)
{
  EXPECT_EQ (GetPhase (*ParseState ("winner_statement: {}")), Phase::FINISHED);

  EXPECT_EQ (GetPhase (*ParseState (R"(
    turn: 1
    winner: 0
  )")), Phase::WINNER_DETERMINED);
}

/* ************************************************************************** */

using WhoseTurnTests = BoardTests;

TEST_F (WhoseTurnTests, TurnSet)
{
  EXPECT_EQ (ParseState (R"(
    turn: 0
    winner: 1
  )")->WhoseTurn (), 0);

  EXPECT_EQ (ParseState (R"(
    turn: 1
    winner: 0
  )")->WhoseTurn (), 1);
}

TEST_F (WhoseTurnTests, TurnNotSet)
{
  EXPECT_EQ (ParseState ("winner_statement: {}")->WhoseTurn (),
             xaya::ParsedBoardState::NO_TURN);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
