// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "board.hpp"

#include "grid.hpp"
#include "proto/boardstate.pb.h"
#include "proto/winnerstatement.pb.h"
#include "testutils.hpp"

#include <gamechannel/proto/metadata.pb.h>
#include <gamechannel/signatures.hpp>
#include <xayagame/testutils.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>
#include <xayautil/uint256.hpp>

#include <json/json.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using testing::_;
using testing::Return;

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

/**
 * Hashes a string to 32 bytes and returns those as string again.
 */
std::string
HashToString (const std::string& preimage)
{
  const xaya::uint256 value = xaya::SHA256::Hash (preimage);
  const char* data = reinterpret_cast<const char*> (value.GetBlob ());
  return std::string (data, xaya::uint256::NUM_BYTES);
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
    p->set_address ("addr 0");

    p = meta.add_participants ();
    p->set_name ("bob");
    p->set_address ("addr 1");
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

class SinglePlayerStateTests : public BoardTests
{

protected:

  SinglePlayerStateTests ()
  {
    meta.mutable_participants ()->RemoveLast ();
    CHECK_EQ (meta.participants_size (), 1);
  }

};

TEST_F (SinglePlayerStateTests, IsValid)
{
  auto p = ParseTextState ("turn: 100", true);
  EXPECT_TRUE (p->IsValid ());
}

TEST_F (SinglePlayerStateTests, WhoseTurn)
{
  EXPECT_EQ (ParseTextState ("turn: 1")->WhoseTurn (),
             xaya::ParsedBoardState::NO_TURN);
}

TEST_F (SinglePlayerStateTests, TurnCount)
{
  EXPECT_EQ (ParseTextState ("winner: 1")->TurnCount (), 0);
}

/* ************************************************************************** */

using InitialBoardStateTests = BoardTests;

TEST_F (InitialBoardStateTests, CorrectInitialState)
{
  const auto actual = InitialBoardState ();
  const auto expected = TextState ("turn: 0");
  EXPECT_TRUE (MessageDifferencer::Equals (actual, expected));
}

TEST_F (InitialBoardStateTests, Phase)
{
  EXPECT_EQ (GetPhase (*ParseState (InitialBoardState ())),
             Phase::FIRST_COMMITMENT);
}

TEST_F (InitialBoardStateTests, WhoseTurn)
{
  EXPECT_EQ (ParseState (InitialBoardState ())->WhoseTurn (), 0);
}

TEST_F (InitialBoardStateTests, TurnCount)
{
  EXPECT_EQ (ParseState (InitialBoardState ())->TurnCount (), 1);
}

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

class ApplyMoveAndTurnCountTests : public BoardTests
{

private:

  jsonrpc::HttpServer httpServer;
  jsonrpc::HttpClient httpClient;

  xaya::MockXayaRpcServer mockXayaServer;

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

  XayaRpcClient rpcClient;

  ApplyMoveAndTurnCountTests ()
    : httpServer(xaya::MockXayaRpcServer::HTTP_PORT),
      httpClient(xaya::MockXayaRpcServer::HTTP_URL),
      mockXayaServer(httpServer),
      rpcClient(httpClient)
  {
    mockXayaServer.StartListening ();
  }

  ~ApplyMoveAndTurnCountTests ()
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
   * the given proto.  This also verifies that the turn count increases by
   * exactly one for the applied move.
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

    EXPECT_EQ (ParseState (oldState)->TurnCount () + 1,
               ParseState (expected)->TurnCount ());
  }

  /**
   * Expects a signature validation call on the mock RPC server for a winner
   * statement on our channel ID, and returns that it is valid with the given
   * signing address.
   */
  void
  ExpectSignature (const std::string& data, const std::string& sgn,
                   const std::string& addr)
  {
    Json::Value res(Json::objectValue);
    res["valid"] = true;
    res["address"] = addr;

    const std::string hashed
        = xaya::GetChannelSignatureMessage (channelId, "winnerstatement", data);
    EXPECT_CALL (mockXayaServer, verifymessage ("", hashed,
                                                xaya::EncodeBase64 (sgn)))
        .WillOnce (Return (res));
  }

};

TEST_F (ApplyMoveAndTurnCountTests, NoCaseSelected)
{
  ExpectInvalid (TextState ("turn: 0"), TextMove (""));
}

/* ************************************************************************** */

using PositionCommitmentTests = ApplyMoveAndTurnCountTests;

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

using SeedRevealTests = ApplyMoveAndTurnCountTests;

TEST_F (SeedRevealTests, InvalidPhase)
{
  proto::BoardMove mv;
  mv.mutable_seed_reveal ()->set_seed ("foobar");

  auto state = TextState (R"(
    turn: 0
  )");
  state.set_seed_hash_0 (HashToString (mv.seed_reveal ().seed ()));

  ExpectInvalid (state, mv);
}

TEST_F (SeedRevealTests, SeedTooLarge)
{
  proto::BoardMove mv;
  mv.mutable_seed_reveal ()->set_seed ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz");

  auto state = TextState (R"(
    turn: 0
    position_hashes: "first hash"
    position_hashes: "second hash"
  )");
  state.set_seed_hash_0 (HashToString (mv.seed_reveal ().seed ()));

  ExpectInvalid (state, mv);
}

TEST_F (SeedRevealTests, NotMatchingCommitment)
{
  ExpectInvalid (TextState (R"(
    turn: 0
    position_hashes: "first hash"
    position_hashes: "second hash"
    seed_hash_0: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
  )"), TextMove (R"(
    seed_reveal:
      {
        seed: "foobar"
      }
  )"));
}

TEST_F (SeedRevealTests, Valid)
{
  for (const std::string seed : {"", "foobar",
                                 "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"})
    {
      proto::BoardMove mv;
      mv.mutable_seed_reveal ()->set_seed (seed);

      auto state = TextState (R"(
        turn: 0
        position_hashes: "first hash"
        position_hashes: "second hash"
        seed_1: "other seed"
      )");
      state.set_seed_hash_0 (HashToString (mv.seed_reveal ().seed ()));

      auto expected = TextState (R"(
        position_hashes: "first hash"
        position_hashes: "second hash"
        known_ships:
          {
            guessed: 0
            hits: 0
          }
        known_ships:
          {
            guessed: 0
            hits: 0
          }
      )");

      xaya::Random rnd;
      rnd.Seed (xaya::SHA256::Hash (seed + "other seed"));
      expected.set_turn (rnd.Next<bool> () ? 1 : 0);

      ExpectNewState (state, mv, expected);
    }
}

TEST_F (SeedRevealTests, MissingSeed1)
{
  proto::BoardMove mv;
  mv.mutable_seed_reveal ()->set_seed ("foo");

  auto state = TextState (R"(
    turn: 0
    position_hashes: "first hash"
    position_hashes: "second hash"
  )");
  state.set_seed_hash_0 (HashToString (mv.seed_reveal ().seed ()));

  auto expected = TextState (R"(
    position_hashes: "first hash"
    position_hashes: "second hash"
    known_ships:
      {
        guessed: 0
        hits: 0
      }
    known_ships:
      {
        guessed: 0
        hits: 0
      }
  )");

  xaya::Random rnd;
  rnd.Seed (xaya::SHA256::Hash ("foo"));
  expected.set_turn (rnd.Next<bool> () ? 1 : 0);

  ExpectNewState (state, mv, expected);
}

/* ************************************************************************** */

class ShotTests : public ApplyMoveAndTurnCountTests
{

protected:

  /** Predefined state in the "shoot" phase.  */
  proto::BoardState state;

  ShotTests ()
  {
    state = TextState (R"(
      turn: 0
      position_hashes: "foo"
      position_hashes: "bar"
      known_ships: {}
      known_ships: {}
    )");
  }

};

TEST_F (ShotTests, InvalidPhase)
{
  ExpectInvalid (TextState ("turn: 0"), TextMove (R"(
    shot:
      {
        location: 42
      }
  )"));
}

TEST_F (ShotTests, NoOrInvalidLocation)
{
  ExpectInvalid (state, TextMove ("shot: {}"));
  ExpectInvalid (state, TextMove ("shot: { location: 64 }"));
}

TEST_F (ShotTests, LocationAlreadyGuessed)
{
  state.mutable_known_ships (1)->set_guessed (2);
  ExpectInvalid (state, TextMove ("shot: { location: 1 }"));
}

TEST_F (ShotTests, ValidShot)
{
  state.mutable_known_ships (0)->set_guessed (1);
  state.mutable_known_ships (1)->set_guessed (2);

  ExpectNewState (state, TextMove ("shot: { location: 0 }"), TextState (R"(
    turn: 1
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: { guessed: 1 }
    known_ships: { guessed: 3 }
    current_shot: 0
  )"));

  state.set_turn (1);
  ExpectNewState (state, TextMove ("shot: { location: 1 }"), TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: { guessed: 3 }
    known_ships: { guessed: 2 }
    current_shot: 1
  )"));
}

/* ************************************************************************** */

class ReplyTests : public ApplyMoveAndTurnCountTests
{

protected:

  /**
   * Predefined state in the "shoot" phase.  By setting a current_shot value,
   * it will be turned into "answer" phase.
   */
  proto::BoardState state;

  ReplyTests ()
  {
    state = TextState (R"(
      turn: 0
      position_hashes: "foo"
      position_hashes: "bar"
      known_ships: {}
      known_ships: {}
    )");
  }

};

TEST_F (ReplyTests, InvalidPhase)
{
  ExpectInvalid (state, TextMove (R"(
    reply:
      {
        reply: HIT
      }
  )"));
}

TEST_F (ReplyTests, NoOrInvalidReply)
{
  state.set_current_shot (42);
  ExpectInvalid (state, TextMove ("reply: {}"));
  ExpectInvalid (state, TextMove ("reply: { reply: INVALID }"));
}

TEST_F (ReplyTests, InvalidCurrentShot)
{
  state.set_current_shot (64);
  ExpectInvalid (state, TextMove ("reply: { reply: MISS }"));
}

TEST_F (ReplyTests, Miss)
{
  const auto miss = TextMove ("reply: { reply: MISS }");

  state.mutable_known_ships (0)->set_hits (5);
  state.mutable_known_ships (1)->set_hits (8);
  state.set_current_shot (10);

  ExpectNewState (state, miss, TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: { hits: 5 }
    known_ships: { hits: 8 }
  )"));

  state.set_turn (1);
  ExpectNewState (state, miss, TextState (R"(
    turn: 1
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: { hits: 5 }
    known_ships: { hits: 8 }
  )"));
}

TEST_F (ReplyTests, Hit)
{
  const auto hit = TextMove ("reply: { reply: HIT }");

  state.mutable_known_ships (0)->set_hits (1);
  state.mutable_known_ships (1)->set_hits (2);

  state.set_turn (0);
  state.set_current_shot (1);
  ExpectNewState (state, hit, TextState (R"(
    turn: 1
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: { hits: 3 }
    known_ships: { hits: 2 }
  )"));

  state.set_turn (1);
  state.set_current_shot (0);
  ExpectNewState (state, hit, TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: { hits: 1 }
    known_ships: { hits: 3 }
  )"));

  /* Here, the state is invalid as it already contains a hit for the given
     shot target.  This should result in an invalid move (and importantly
     no crash or CHECK failure).  */
  state.set_turn (0);
  state.set_current_shot (0);
  ExpectInvalid (state, hit);
}

/* ************************************************************************** */

class PositionRevealTests : public ApplyMoveAndTurnCountTests
{

protected:

  /**
   * A BoardState instance that is used for testing.  It is initialised to
   * some basic value in the constructor, namely with two empty known_ships
   * fields set.  The position_hashes should be set using CommitPosition,
   * and other fields as well as needed.
   */
  proto::BoardState state;

  /** A valid position of ships.  */
  uint64_t validPosition;

  PositionRevealTests ()
  {
    state = TextState (R"(
      known_ships: {}
      known_ships: {}
    )");

    const Grid validGrid = GridFromString (
      "xxxx..xx"
      "........"
      "......xx"
      "........"
      "......xx"
      "x.x....."
      "x.x...xx"
      "x.x....."
    );
    CHECK (VerifyPositionOfShips (validGrid));
    validPosition = validGrid.GetBits ();
  }

  /**
   * Adds a position_hashes field to state, based on the given position int
   * and salt.
   */
  void
  CommitPosition (uint64_t position, const std::string& salt)
  {
    std::ostringstream data;
    for (int i = 0; i < 8; ++i)
      {
        data << static_cast<char> (position & 0xFF);
        position >>= 8;
      }
    data << salt;

    state.add_position_hashes (HashToString (data.str ()));
  }

  /**
   * Utility method that returns a BoardMove proto, revealing the
   * "validPosition" with the given salt.
   */
  proto::BoardMove
  ValidPositionMove (const std::string& salt)
  {
    proto::BoardMove mv;

    auto* reveal = mv.mutable_position_reveal ();
    reveal->set_position (validPosition);
    reveal->set_salt (salt);

    return mv;
  }

};

TEST_F (PositionRevealTests, InvalidPhase)
{
  ExpectInvalid (TextState ("turn: 0"), TextMove (R"(
    position_reveal:
      {
        position: 42
      }
  )"));
}

TEST_F (PositionRevealTests, InvalidMoveProto)
{
  state.set_turn (0);
  CommitPosition (10, "");
  CommitPosition (15, "");

  ExpectInvalid (state, TextMove (R"(
    position_reveal:
      {
        salt: "foo"
      }
  )"));
  ExpectInvalid (state, TextMove (R"(
    position_reveal:
      {
        position: 42
        salt: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz"
      }
  )"));
}

TEST_F (PositionRevealTests, CommitmentMismatch)
{
  state.set_turn (0);
  CommitPosition (10, "foo");
  CommitPosition (42, "bar");

  ExpectInvalid (state, TextMove (R"(
    position_reveal:
      {
        position: 42
        salt: "bar"
      }
  )"));
}

TEST_F (PositionRevealTests, MissingSaltOk)
{
  state.set_turn (0);
  CommitPosition (10, "");
  CommitPosition (20, "");

  ExpectNewState (state, TextMove (R"(
    position_reveal:
      {
        position: 10
      }
  )"), TextState (R"(
    turn: 0
    winner: 1
    position_hashes: ""
    position_hashes: ""
    known_ships: {}
    known_ships: {}
    positions: 10
    positions: 0
  )"));
}

TEST_F (PositionRevealTests, HitsNotSubsetOfGuesses)
{
  state.set_turn (0);
  CommitPosition (validPosition, "");
  CommitPosition (validPosition, "");

  state.mutable_known_ships (0)->set_hits (1);

  ExpectInvalid (state, ValidPositionMove (""));
}

TEST_F (PositionRevealTests, InvalidShipConfiguration)
{
  state.set_turn (0);
  state.set_current_shot (42);
  CommitPosition (10, "foo");
  CommitPosition (20, "bar");

  ExpectNewState (state, TextMove (R"(
    position_reveal:
      {
        position: 10
        salt: "foo"
      }
  )"), TextState (R"(
    turn: 0
    winner: 1
    current_shot: 42
    position_hashes: ""
    position_hashes: ""
    known_ships: {}
    known_ships: {}
    positions: 10
    positions: 0
  )"));
}

TEST_F (PositionRevealTests, ShotReplyMismatches)
{
  state.set_turn (1);
  state.add_position_hashes ("");
  CommitPosition (validPosition, "bar");
  state.add_positions (42);
  state.add_positions (0);
  state.mutable_known_ships (1)->set_guessed (validPosition);

  auto expected = TextState (R"(
    turn: 1
    winner: 0
    position_hashes: ""
    position_hashes: ""
    positions: 42
  )");
  *expected.mutable_known_ships () = state.known_ships ();
  expected.add_positions (validPosition);

  ExpectNewState (state, ValidPositionMove ("bar"), expected);
}

TEST_F (PositionRevealTests, AllShipsHit)
{
  state.set_turn (0);
  CommitPosition (validPosition, "foo");
  CommitPosition (42, "bar");

  /* This is not a valid ship configuration, but it contains enough hits
     to count as "all ships sunk".  */
  state.mutable_known_ships (1)->set_guessed (0xFFFFFFFF);
  state.mutable_known_ships (1)->set_hits (0xFFFFFF00);

  auto expected = TextState  (R"(
    turn: 1
    winner: 0
    position_hashes: ""
    position_hashes: ""
  )");
  *expected.mutable_known_ships () = state.known_ships ();
  expected.add_positions (validPosition);
  expected.add_positions (0);

  ExpectNewState (state, ValidPositionMove ("foo"), expected);
}

TEST_F (PositionRevealTests, NotAllShipsHitAfterFirst)
{
  state.set_turn (1);
  state.add_position_hashes ("first hash");
  CommitPosition (validPosition, "bar");

  auto expected = TextState  (R"(
    turn: 0
    position_hashes: "first hash"
    position_hashes: ""
    known_ships: {}
    known_ships: {}
    positions: 0
  )");
  expected.add_positions (validPosition);

  ExpectNewState (state, ValidPositionMove ("bar"), expected);
}

TEST_F (PositionRevealTests, NotAllShipsHitSecondWins)
{
  state.set_turn (0);
  CommitPosition (validPosition, "foo");
  state.add_position_hashes ("");
  state.add_positions (0);
  state.add_positions (1);

  auto expected = TextState (R"(
    turn: 1
    winner: 0
    position_hashes: ""
    position_hashes: ""
    known_ships: {}
    known_ships: {}
    positions: 0
    positions: 1
  )");
  expected.set_positions (0, validPosition);

  ExpectNewState (state, ValidPositionMove ("foo"), expected);
}

/* ************************************************************************** */

class WinnerStatementTests : public ApplyMoveAndTurnCountTests
{

protected:

  /**
   * Returns a SignedData proto holding a winner-statement parsed from
   * text and with the given signatures.
   */
  static xaya::proto::SignedData
  SignedWinnerStatement (const std::string& stmtStr,
                         const std::vector<std::string>& signatures = {})
  {
    proto::WinnerStatement stmt;
    CHECK (TextFormat::ParseFromString (stmtStr, &stmt));

    xaya::proto::SignedData res;
    CHECK (stmt.SerializeToString (res.mutable_data ()));

    for (const auto& sgn : signatures)
      res.add_signatures (sgn);

    return res;
  }

  /**
   * Returns a winner-statement move proto where the statement itself
   * is given as text proto.
   */
  static proto::BoardMove
  WinnerStatementMove (const std::string& stmtStr,
                       const std::vector<std::string>& signatures = {})
  {
    proto::BoardMove res;
    auto* signedData = res.mutable_winner_statement ()->mutable_statement ();
    *signedData = SignedWinnerStatement (stmtStr, signatures);

    return res;
  }

};

using VerifySignedWinnerStatementTests = WinnerStatementTests;

TEST_F (VerifySignedWinnerStatementTests, MissingData)
{
  const auto data = SignedWinnerStatement ("");

  proto::WinnerStatement stmt;
  EXPECT_FALSE (VerifySignedWinnerStatement (rpcClient, channelId, meta,
                                             data, stmt));
}

TEST_F (VerifySignedWinnerStatementTests, MalformedData)
{
  xaya::proto::SignedData data;
  data.set_data ("invalid proto");

  proto::WinnerStatement stmt;
  EXPECT_FALSE (VerifySignedWinnerStatement (rpcClient, channelId, meta,
                                             data, stmt));
}

TEST_F (VerifySignedWinnerStatementTests, NoWinnerGiven)
{
  const auto data = SignedWinnerStatement ("");

  proto::WinnerStatement stmt;
  EXPECT_FALSE (VerifySignedWinnerStatement (rpcClient, channelId, meta,
                                             data, stmt));
}

TEST_F (VerifySignedWinnerStatementTests, InvalidWinnerGiven)
{
  const auto data = SignedWinnerStatement ("winner: 2");

  proto::WinnerStatement stmt;
  EXPECT_FALSE (VerifySignedWinnerStatement (rpcClient, channelId, meta,
                                             data, stmt));
}

TEST_F (VerifySignedWinnerStatementTests, InvalidSignature)
{
  const auto data = SignedWinnerStatement ("winner: 0", {"sgn 0"});
  ExpectSignature (data.data (), "sgn 0",  "addr 0");

  proto::WinnerStatement stmt;
  EXPECT_FALSE (VerifySignedWinnerStatement (rpcClient, channelId, meta,
                                             data, stmt));
}

TEST_F (VerifySignedWinnerStatementTests, Valid)
{
  const auto data = SignedWinnerStatement ("winner: 1", {"sgn 0"});
  ExpectSignature (data.data (), "sgn 0",  "addr 0");

  proto::WinnerStatement stmt;
  ASSERT_TRUE (VerifySignedWinnerStatement (rpcClient, channelId, meta,
                                            data, stmt));
  EXPECT_EQ (stmt.winner (), 1);
}

using WinnerStatementMoveTests = WinnerStatementTests;

TEST_F (WinnerStatementMoveTests, InvalidPhase)
{
  ExpectInvalid (TextState ("turn: 0"), WinnerStatementMove ("winner: 1"));
}

TEST_F (WinnerStatementMoveTests, MissingStatement)
{
  ExpectInvalid (TextState (R"(
    turn: 0
    winner: 1
  )"), TextMove ("winner_statement: {}"));

}

TEST_F (WinnerStatementMoveTests, InvalidWinner)
{
  auto mv = WinnerStatementMove ("winner: 0", {"sgn 1"});
  ExpectSignature (mv.winner_statement ().statement ().data (),
                   "sgn 1",  "addr 1");
  ExpectInvalid (TextState (R"(
    turn: 0
    winner: 1
  )"), mv);

  mv = WinnerStatementMove ("winner: 1", {"sgn 0"});
  ExpectSignature (mv.winner_statement ().statement ().data (),
                   "sgn 0",  "addr 0");
  ExpectInvalid (TextState (R"(
    turn: 1
    winner: 0
  )"), mv);
}

TEST_F (WinnerStatementMoveTests, Valid)
{
  const auto mv = WinnerStatementMove ("winner: 1", {"sgn 0"});
  ExpectSignature (mv.winner_statement ().statement ().data (),
                   "sgn 0",  "addr 0");

  auto expected = TextState (R"(
    winner: 1
  )");
  *expected.mutable_winner_statement () = mv.winner_statement ().statement ();

  ExpectNewState (TextState (R"(
    turn: 0
    winner: 1
  )"), mv, expected);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
