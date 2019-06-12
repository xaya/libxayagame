// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "board.hpp"

#include "proto/boardstate.pb.h"

#include <gamechannel/proto/metadata.pb.h>
#include <xayagame/testutils.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayautil/hash.hpp>
#include <xayautil/uint256.hpp>

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gtest/gtest.h>

#include <memory>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;

namespace ships
{

namespace
{

/**
 * Parses a text-format state proto.
 */
proto::BoardState
TextState (const std::string& str)
{
  proto::BoardState res;
  CHECK (TextFormat::ParseFromString (str, &res));
  return res;
}

/**
 * Parses a text-format move proto.
 */
proto::BoardMove
TextMove (const std::string& str)
{
  proto::BoardMove res;
  CHECK (TextFormat::ParseFromString (str, &res));
  return res;
}

/* Allow printing as text proto for logging.  */

template <typename S>
  S&
  operator<< (S& out, const proto::BoardState& pb)
{
  std::string str;
  CHECK (TextFormat::PrintToString (pb, &str));
  return out << str;
}

} // anonymous namespace

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
   * Parses a BoardState given as proto into a ShipsBoardState
   * instance.
   */
  std::unique_ptr<ShipsBoardState>
  ParseState (const proto::BoardState& pb, const bool allowInvalid = false)
  {
    std::string serialised;
    CHECK (pb.SerializeToString (&serialised));

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
   * Utility method that parses a text-proto state.
   */
  std::unique_ptr<ShipsBoardState>
  ParseTextState (const std::string& str, const bool allowInvalid = false)
  {
    return ParseState (TextState (str), allowInvalid);
  }

  /**
   * Exposes ShipsBoardState::GetPhase to subtests.
   */
  static Phase
  GetPhase (const ShipsBoardState& s)
  {
    return s.GetPhase ();
  }

  /**
   * Exposes ShipsBoardState::ApplyMoveProto to subtests.
   */
  static bool
  ApplyMoveProto (const ShipsBoardState& s, XayaRpcClient& rpc,
                  const proto::BoardMove& mv, proto::BoardState& newState)
  {
    return s.ApplyMoveProto (rpc, mv, newState);
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

    auto p = ParseTextState (str, true);
    ASSERT_NE (p, nullptr);
    EXPECT_TRUE (p->IsValid ());
  }

  void
  ExpectInvalid (const std::string& str)
  {
    LOG (INFO) << "Expecting state to be invalid: " << str;
    EXPECT_EQ (ParseTextState (str, true), nullptr);
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
  EXPECT_EQ (GetPhase (*ParseTextState ("turn: 0")), Phase::FIRST_COMMITMENT);

  EXPECT_EQ (GetPhase (*ParseTextState (R"(
    turn: 1
    position_hashes: "foo"
  )")), Phase::SECOND_COMMITMENT);
}

TEST_F (GetPhaseTests, RevealSeed)
{
  EXPECT_EQ (GetPhase (*ParseTextState (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
  )")), Phase::FIRST_REVEAL_SEED);
}

TEST_F (GetPhaseTests, ShotAndAnswer)
{
  EXPECT_EQ (GetPhase (*ParseTextState (R"(
    turn: 0
    position_hashes: "a"
    position_hashes: "b"
    known_ships: {}
    known_ships: {}
  )")), Phase::SHOOT);

  EXPECT_EQ (GetPhase (*ParseTextState (R"(
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
  EXPECT_EQ (GetPhase (*ParseTextState (R"(
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
  EXPECT_EQ (GetPhase (*ParseTextState ("winner_statement: {}")),
             Phase::FINISHED);

  EXPECT_EQ (GetPhase (*ParseTextState (R"(
    turn: 1
    winner: 0
  )")), Phase::WINNER_DETERMINED);
}

/* ************************************************************************** */

using WhoseTurnTests = BoardTests;

TEST_F (WhoseTurnTests, TurnSet)
{
  EXPECT_EQ (ParseTextState (R"(
    turn: 0
    winner: 1
  )")->WhoseTurn (), 0);

  EXPECT_EQ (ParseTextState (R"(
    turn: 1
    winner: 0
  )")->WhoseTurn (), 1);
}

TEST_F (WhoseTurnTests, TurnNotSet)
{
  EXPECT_EQ (ParseTextState ("winner_statement: {}")->WhoseTurn (),
             xaya::ParsedBoardState::NO_TURN);
}

/* ************************************************************************** */

class ApplyMoveTests : public BoardTests
{

private:

  jsonrpc::HttpServer httpServer;
  jsonrpc::HttpClient httpClient;

  xaya::MockXayaRpcServer mockXayaServer;
  XayaRpcClient rpcClient;

  /**
   * Calls ApplyMoveProto with a given move onto a given state, both as
   * proto instances.
   */
  bool
  ApplyMove (const proto::BoardState& state, const proto::BoardMove& mv,
             proto::BoardState& newState)
  {
    auto oldState = ParseState (state);
    CHECK (oldState != nullptr) << "Old state is invalid: " << state;

    return ApplyMoveProto (*oldState, rpcClient, mv, newState);
  }

protected:

  ApplyMoveTests ()
    : httpServer(xaya::MockXayaRpcServer::HTTP_PORT),
      httpClient(xaya::MockXayaRpcServer::HTTP_URL),
      mockXayaServer(httpServer),
      rpcClient(httpClient)
  {
    mockXayaServer.StartListening ();
  }

  ~ApplyMoveTests ()
  {
    mockXayaServer.StopListening ();
  }

  /**
   * Tries to apply a move onto the given state and expects that it is invalid.
   */
  void
  ExpectInvalid (const proto::BoardState& oldState, const proto::BoardMove& mv)
  {
    proto::BoardState newState;
    EXPECT_FALSE (ApplyMove (oldState, mv, newState));
  }

  /**
   * Applies a move onto the given state and expects that the new state matches
   * the given proto.
   */
  void
  ExpectNewState (const proto::BoardState& oldState, const proto::BoardMove& mv,
                  const proto::BoardState& expected)
  {
    proto::BoardState actual;
    ASSERT_TRUE (ApplyMove (oldState, mv, actual));

    EXPECT_TRUE (MessageDifferencer::Equals (actual, expected))
        << "Actual new game state: " << actual
        << "\n  does not equal expected new state: " << expected;
  }

};

TEST_F (ApplyMoveTests, NoCaseSelected)
{
  ExpectInvalid (TextState ("turn: 0"), TextMove (""));
}

/* ************************************************************************** */

using PositionCommitmentTests = ApplyMoveTests;

TEST_F (PositionCommitmentTests, InvalidPositionHash)
{
  const auto oldStateFirst = TextState ("turn: 0");
  ExpectInvalid (oldStateFirst, TextMove ("position_commitment: {}"));
  ExpectInvalid (oldStateFirst, TextMove (R"(
    position_commitment:
      {
        position_hash: "x"
      }
  )"));
}

TEST_F (PositionCommitmentTests, InWrongPhase)
{
  ExpectInvalid (TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      }
  )"));
}

TEST_F (PositionCommitmentTests, ValidFirstCommitment)
{
  ExpectNewState (TextState (R"(
    turn: 0
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        seed_hash: "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
      }
  )"), TextState (R"(
    turn: 1
    position_hashes: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    seed_hash_0: "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
  )"));
}

TEST_F (PositionCommitmentTests, InvalidFirstCommitment)
{
  ExpectInvalid (TextState (R"(
    turn: 0
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        seed_hash: "foo"
      }
  )"));

  ExpectInvalid (TextState (R"(
    turn: 0
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        seed_hash: "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        seed: ""
      }
  )"));
}

TEST_F (PositionCommitmentTests, ValidSecondCommitment)
{
  ExpectNewState (TextState (R"(
    turn: 1
    position_hashes: "first hash"
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        seed: "abc"
      }
  )"), TextState (R"(
    turn: 0
    position_hashes: "first hash"
    position_hashes: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    seed_1: "abc"
  )"));

  ExpectNewState (TextState (R"(
    turn: 1
    position_hashes: "first hash"
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      }
  )"), TextState (R"(
    turn: 0
    position_hashes: "first hash"
    position_hashes: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    seed_1: ""
  )"));

  ExpectNewState (TextState (R"(
    turn: 1
    position_hashes: "first hash"
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        seed: "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
      }
  )"), TextState (R"(
    turn: 0
    position_hashes: "first hash"
    position_hashes: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    seed_1: "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
  )"));
}

TEST_F (PositionCommitmentTests, InvalidSecondCommitment)
{
  ExpectInvalid (TextState (R"(
    turn: 1
    position_hashes: "first hash"
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        seed: "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyz"
      }
  )"));

  ExpectInvalid (TextState (R"(
    turn: 1
    position_hashes: "first hash"
  )"), TextMove (R"(
    position_commitment:
      {
        position_hash: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        seed: "abc"
        seed_hash: "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
      }
  )"));
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
