// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channel.hpp"

#include "board.hpp"
#include "proto/boardstate.pb.h"
#include "proto/winnerstatement.pb.h"
#include "testutils.hpp"

#include <gamechannel/protoutils.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayagame/testutils.hpp>
#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace ships
{
namespace
{

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using testing::_;
using testing::Return;
using testing::Throw;
using testing::Truly;

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
 * Parses a text-format state proof proto.
 */
xaya::proto::StateProof
TextProof (const std::string& str)
{
  xaya::proto::StateProof res;
  CHECK (TextFormat::ParseFromString (str, &res));
  return res;
}

/**
 * Builds a winner statement with (fake) signatures for the given winner.
 */
xaya::proto::SignedData
FakeWinnerStatement (const int winner)
{
  proto::WinnerStatement stmt;
  stmt.set_winner (winner);

  xaya::proto::SignedData res;
  CHECK (stmt.SerializeToString (res.mutable_data ()));
  res.add_signatures ("sgn");

  return res;
}

class ChannelTests : public testing::Test
{

private:

  jsonrpc::HttpServer httpServerWallet;
  jsonrpc::HttpClient httpClientWallet;

protected:

  const xaya::uint256 channelId = xaya::SHA256::Hash ("foo");

  /**
   * Two metadata instances, where "we" are either the first or second player.
   */
  xaya::proto::ChannelMetadata meta[2];

  xaya::MockXayaWalletRpcServer mockXayaWallet;
  XayaWalletRpcClient rpcWallet;

  ShipsBoardRules rules;
  ShipsChannel channel;

  ChannelTests ()
    : httpServerWallet(xaya::MockXayaWalletRpcServer::HTTP_PORT),
      httpClientWallet(xaya::MockXayaWalletRpcServer::HTTP_URL),
      mockXayaWallet(httpServerWallet),
      rpcWallet(httpClientWallet),
      channel(rpcWallet, "player")
  {
    CHECK (TextFormat::ParseFromString (R"(
      participants:
        {
          name: "player"
          address: "my addr"
        }
      participants:
        {
          name: "other player"
          address: "other addr"
        }
    )", &meta[0]));

    CHECK (TextFormat::ParseFromString (R"(
      participants:
        {
          name: "other player"
          address: "other addr"
        }
      participants:
        {
          name: "player"
          address: "my addr"
        }
    )", &meta[1]));

    mockXayaWallet.StartListening ();
  }

  ~ChannelTests ()
  {
    mockXayaWallet.StopListening ();
  }

  /**
   * Parses a BoardState proto into a ParsedBoardState.  This automatically
   * associates the correct metadata instance, where the current player
   * is the one to play next.
   */
  std::unique_ptr<xaya::ParsedBoardState>
  ParseState (const proto::BoardState& pb)
  {
    /* In some situations, pb.turn might not be set.  But then we just use
       the default value of zero, which is fine for those.  */
    return ParseState (pb, meta[pb.turn ()]);
  }

  /**
   * Parses a BoardState proto into a ParsedBoardState, using the given
   * metadata instance.
   */
  std::unique_ptr<xaya::ParsedBoardState>
  ParseState (const proto::BoardState& pb,
              const xaya::proto::ChannelMetadata& m)
  {
    std::string serialised;
    CHECK (pb.SerializeToString (&serialised));

    auto res = rules.ParseState (channelId, m, serialised);
    CHECK (res != nullptr);

    return res;
  }

};

/* ************************************************************************** */

class OnChainMoveTests : public ChannelTests
{


protected:

  xaya::MoveSender sender;

  OnChainMoveTests ()
    : sender("xs", channelId, "player", rpcWallet, channel)
  {}

  /**
   * Verifies that a given JSON object matches the expected move format
   * for the given key ("r", "d" or "w"), channel ID and encoded data proto.
   * Note that all three types of moves (disputes, resolutions and channel
   * closes with WinnerStatement's) have the same basic structure.
   */
  template <typename Proto>
    static bool
    IsExpectedMove (const Json::Value& actual,
                    const std::string& key, const std::string& protoKey,
                    const xaya::uint256& id, const Proto& expectedPb)
  {
    if (!actual.isObject ())
      return false;
    if (actual.size () != 1)
      return false;

    const auto& sub = actual[key];
    if (!sub.isObject ())
      return false;
    if (sub.size () != 2)
      return false;

    if (!sub["id"].isString ())
      return false;
    if (sub["id"].asString () != id.ToHex ())
      return false;

    if (!sub[protoKey].isString ())
      return false;

    Proto actualPb;
    if (!xaya::ProtoFromBase64 (sub[protoKey].asString (), actualPb))
      return false;

    return MessageDifferencer::Equals (actualPb, expectedPb);
  }

};

TEST_F (OnChainMoveTests, ResolutionMove)
{
  const auto proof = TextProof (R"(
    initial_state:
      {
        data: ""
        signatures: "sgn 0"
      }
  )");

  EXPECT_TRUE (IsExpectedMove (channel.ResolutionMove (channelId, proof),
                               "r", "state", channelId, proof));
}

TEST_F (OnChainMoveTests, DisputeMove)
{
  const auto proof = TextProof (R"(
    initial_state:
      {
        data: ""
        signatures: "sgn 0"
      }
  )");

  EXPECT_TRUE (IsExpectedMove (channel.DisputeMove (channelId, proof),
                               "d", "state", channelId, proof));
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveNotFinished)
{
  channel.MaybeOnChainMove (*ParseState (TextState ("turn: 0")), sender);
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveNotMe)
{
  proto::BoardState state;
  *state.mutable_winner_statement () = FakeWinnerStatement (1);

  channel.MaybeOnChainMove (*ParseState (state), sender);
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveSending)
{
  const auto stmt = FakeWinnerStatement (0);

  const auto isOk = [this, stmt] (const std::string& str)
    {
      const auto val = ParseJson (str);
      const auto& mv = val["g"]["xs"];
      return IsExpectedMove (mv, "w", "stmt", channelId, stmt);
    };
  EXPECT_CALL (mockXayaWallet, name_update ("p/player", Truly (isOk)))
      .WillOnce (Return (xaya::SHA256::Hash ("txid").ToHex ()));

  proto::BoardState state;
  *state.mutable_winner_statement () = stmt;

  channel.MaybeOnChainMove (*ParseState (state), sender);
}

/* ************************************************************************** */

using PositionStoringTests = ChannelTests;

TEST_F (PositionStoringTests, SetPosition)
{
  ASSERT_FALSE (channel.IsPositionSet ());
  channel.SetPosition (GridFromString (
    "xxxx...."
    "........"
    "xxx....."
    "........"
    "xxx....."
    "........"
    ".x.x.x.x"
    ".x.x.x.x"
  ));
  EXPECT_TRUE (channel.IsPositionSet ());
}

TEST_F (PositionStoringTests, InvalidPosition)
{
  channel.SetPosition (GridFromString (
    "xxxx...."
    "........"
    "xxx....."
    "........"
    "xxx....."
    "........"
    "........"
    "........"
  ));
  EXPECT_FALSE (channel.IsPositionSet ());
}

/* ************************************************************************** */

/**
 * Basic tests for automoves with Xayaships.  Those verify only some situations
 * including edge cases.  Other verification (e.g. that the actual hash values
 * work fine with revealing later) is done separately with tests that run
 * a full board game through the move processor.
 */
class AutoMoveTests : public ChannelTests
{

protected:

  /** Some valid ships position.  */
  Grid validPosition;

  AutoMoveTests ()
  {
    validPosition = GridFromString (
      "xxxx...."
      "........"
      "xxx....."
      "........"
      "xxx....."
      "........"
      ".x.x.x.x"
      ".x.x.x.x"
    );
  }

  /**
   * Calls MaybeAutoMove on our channel and verifies that there is no automove.
   */
  void
  ExpectNoAutoMove (const xaya::ParsedBoardState& state)
  {
    xaya::BoardMove mv;
    ASSERT_FALSE (channel.MaybeAutoMove (state, mv));
  }

  /**
   * Calls MaybeAutoMove on our channel, verifies that there is an automove,
   * and returns the resulting proto.
   */
  proto::BoardMove
  ExpectAutoMove (const xaya::ParsedBoardState& state)
  {
    xaya::BoardMove mv;
    if (!channel.MaybeAutoMove (state, mv))
      {
        ADD_FAILURE () << "No auto move provided, expected one";
        return proto::BoardMove ();
      }

    proto::BoardMove res;
    CHECK (res.ParseFromString (mv));

    return res;
  }

};

TEST_F (AutoMoveTests, FirstPositionCommitmentNotYetSet)
{
  ExpectNoAutoMove (*ParseState (TextState ("turn: 0")));
}

TEST_F (AutoMoveTests, FirstPositionCommitmentOk)
{
  channel.SetPosition (validPosition);

  const auto mv = ExpectAutoMove (*ParseState (TextState ("turn: 0")));
  ASSERT_TRUE (mv.has_position_commitment ());
  EXPECT_TRUE (mv.position_commitment ().has_position_hash ());
  EXPECT_TRUE (mv.position_commitment ().has_seed_hash ());
  EXPECT_FALSE (mv.position_commitment ().has_seed ());
}

TEST_F (AutoMoveTests, SecondPositionCommitmentNotYetSet)
{
  ExpectNoAutoMove (*ParseState (TextState (R"(
    turn: 1
    position_hashes: "foo"
  )")));
}

TEST_F (AutoMoveTests, SecondPositionCommitmentOk)
{
  channel.SetPosition (validPosition);

  const auto mv = ExpectAutoMove (*ParseState (TextState (R"(
    turn: 1
    position_hashes: "foo"
  )")));
  ASSERT_TRUE (mv.has_position_commitment ());
  EXPECT_TRUE (mv.position_commitment ().has_position_hash ());
  EXPECT_FALSE (mv.position_commitment ().has_seed_hash ());
  EXPECT_EQ (mv.position_commitment ().seed ().size (), 32);
}

TEST_F (AutoMoveTests, FirstRevealSeed)
{
  /* Perform a position commitment first, so that we initialise the seed
     randomly.  Then we can verify it was really set and not just to
     an empty string.  */
  channel.SetPosition (validPosition);
  ExpectAutoMove (*ParseState (TextState ("turn: 0")));

  const auto mv = ExpectAutoMove (*ParseState (TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
  )")));
  ASSERT_TRUE (mv.has_seed_reveal ());
  EXPECT_EQ (mv.seed_reveal ().seed ().size (), 32);
}

TEST_F (AutoMoveTests, ShootNotAllHit)
{
  ExpectNoAutoMove (*ParseState (TextState (R"(
    turn: 1
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: {}
    known_ships: {}
  )")));
}

TEST_F (AutoMoveTests, ShootAllShipsHit)
{
  channel.SetPosition (validPosition);

  const Grid allAndMore = GridFromString (
      "xxxx...x"
      ".......x"
      "xxx....x"
      ".......x"
      "xxx....x"
      "........"
      ".x.x.x.x"
      ".x.x.x.x"
  );

  auto statePb = TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: {}
    known_ships: {}
  )");
  statePb.mutable_known_ships (1)->set_hits (allAndMore.GetBits ());

  const auto mv = ExpectAutoMove (*ParseState (statePb));
  ASSERT_TRUE (mv.has_position_reveal ());
  EXPECT_EQ (mv.position_reveal ().salt ().size (), 32);
}

TEST_F (AutoMoveTests, Answer)
{
  channel.SetPosition (validPosition);

  auto statePb = TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: {}
    known_ships: {}
  )");

  statePb.set_current_shot (0);
  auto mv = ExpectAutoMove (*ParseState (statePb));
  ASSERT_TRUE (mv.has_reply ());
  EXPECT_EQ (mv.reply ().reply (), proto::ReplyMove::HIT);

  statePb.set_current_shot (7);
  mv = ExpectAutoMove (*ParseState (statePb));
  ASSERT_TRUE (mv.has_reply ());
  EXPECT_EQ (mv.reply ().reply (), proto::ReplyMove::MISS);
}

TEST_F (AutoMoveTests, SecondRevealPosition)
{
  channel.SetPosition (validPosition);

  const auto mv = ExpectAutoMove (*ParseState (TextState (R"(
    turn: 0
    position_hashes: "foo"
    position_hashes: "bar"
    known_ships: {}
    known_ships: {}
    positions: 0
    positions: 42
  )")));
  ASSERT_TRUE (mv.has_position_reveal ());
  EXPECT_EQ (mv.position_reveal ().salt ().size (), 32);
}

TEST_F (AutoMoveTests, WinnerDeterminedSignatureFailure)
{
  EXPECT_CALL (mockXayaWallet, signmessage ("my addr", _))
      .WillOnce (Throw (jsonrpc::JsonRpcException (-5)));

  ExpectNoAutoMove (*ParseState (TextState (R"(
    turn: 0
    winner: 1
  )")));
}

TEST_F (AutoMoveTests, WinnerDeterminedOk)
{
  EXPECT_CALL (mockXayaWallet, signmessage ("my addr", _))
      .WillOnce (Return (xaya::EncodeBase64 ("sgn")));

  const auto mv = ExpectAutoMove (*ParseState (TextState (R"(
    turn: 0
    winner: 1
  )")));
  ASSERT_TRUE (mv.has_winner_statement ());
  const auto& data = mv.winner_statement ().statement ();
  EXPECT_EQ (data.signatures_size (), 1);
  EXPECT_EQ (data.signatures (0), "sgn");

  proto::WinnerStatement stmt;
  ASSERT_TRUE (stmt.ParseFromString (data.data ()));
  EXPECT_EQ (stmt.winner (), 1);
}

/* ************************************************************************** */

/**
 * Tests that run a full game between two channel instances, just like
 * it would be done with automoves and real frontends.
 */
class FullGameTests : public ChannelTests
{

private:

  jsonrpc::HttpServer httpServer;
  jsonrpc::HttpClient httpClient;

  xaya::MockXayaRpcServer mockXayaServer;
  XayaRpcClient rpcClient;

  /** Indexable array of the channels.  */
  ShipsChannel* channels[2];

protected:

  ShipsChannel otherChannel;

  /** The current game state.  This is updated as moves are made.  */
  std::unique_ptr<xaya::ParsedBoardState> state;

  FullGameTests ()
    : httpServer(xaya::MockXayaRpcServer::HTTP_PORT),
      httpClient(xaya::MockXayaRpcServer::HTTP_URL),
      mockXayaServer(httpServer),
      rpcClient(httpClient),
      otherChannel(rpcWallet, "other player")
  {
    channels[0] = &channel;
    channels[1] = &otherChannel;

    state = ParseState (InitialBoardState (), meta[0]);

    mockXayaServer.StartListening ();

    /* Set up the mock RPC servers so that we can "validate" signatures
       and "sign" messages.  */
    EXPECT_CALL (mockXayaWallet, signmessage ("my addr", _))
        .WillRepeatedly (Return (xaya::EncodeBase64 ("my sgn")));
    EXPECT_CALL (mockXayaWallet, signmessage ("other addr", _))
        .WillRepeatedly (Return (xaya::EncodeBase64 ("other sgn")));

    Json::Value res(Json::objectValue);
    res["valid"] = true;
    res["address"] = "my addr";
    EXPECT_CALL (mockXayaServer,
                 verifymessage ("", _, xaya::EncodeBase64 ("my sgn")))
        .WillRepeatedly (Return (res));

    res["address"] = "other addr";
    EXPECT_CALL (mockXayaServer,
                 verifymessage ("", _, xaya::EncodeBase64 ("other sgn")))
        .WillRepeatedly (Return (res));
  }

  ~FullGameTests ()
  {
    mockXayaServer.StopListening ();
  }

  /**
   * Sets up the positions of both channels.  They are chosen in such a way
   * that "channel" wins when guesses are made in increasing order (0, 1, ...),
   * i.e. the ships of "channel" are more towards the "higher coordinates".
   */
  void
  SetupPositions ()
  {
    channel.SetPosition (GridFromString (
      "........"
      "........"
      "........"
      "xx.xx.xx"
      "........"
      "..xx.xxx"
      "........"
      "xxx.xxxx"
    ));
    otherChannel.SetPosition (GridFromString (
      "xx.xx.xx"
      "........"
      "..xx.xxx"
      "........"
      "xxx.xxxx"
      "........"
      "........"
      "........"
    ));

    CHECK (channel.IsPositionSet ());
    CHECK (otherChannel.IsPositionSet ());
  }

  /**
   * Returns the channel reference for the player whose turn it is.
   */
  ShipsChannel&
  GetCurrentChannel ()
  {
    const int turn = state->WhoseTurn ();
    CHECK_NE (turn, xaya::ParsedBoardState::NO_TURN);
    return *channels[turn];
  }

  /**
   * Updates the current board state with the given move.
   */
  void
  ProcessMove (const proto::BoardMove& mv)
  {
    xaya::BoardMove serialised;
    CHECK (mv.SerializeToString (&serialised));

    xaya::BoardState newState;
    CHECK (state->ApplyMove (rpcClient, serialised, newState));

    state = rules.ParseState (channelId, meta[0], newState);
    CHECK (state != nullptr);
  }

  /**
   * Processes all automoves that can be processed.  Returns true if some
   * moves were made.
   */
  bool
  ProcessAuto ()
  {
    bool res = false;
    while (true)
      {
        if (state->WhoseTurn () == xaya::ParsedBoardState::NO_TURN)
          return res;

        xaya::BoardMove mv;
        if (!GetCurrentChannel ().MaybeAutoMove (*state, mv))
          return res;

        proto::BoardMove mvPb;
        CHECK (mvPb.ParseFromString (mv));

        ProcessMove (mvPb);
        res = true;
      }
  }

  /**
   * Expects that the game is finished and the given player won.
   */
  void
  ExpectWinner (const int winner) const
  {
    ASSERT_EQ (state->WhoseTurn (), xaya::ParsedBoardState::NO_TURN);

    const auto& shipsState = dynamic_cast<const ShipsBoardState&> (*state);
    const auto& pb = shipsState.GetState ();

    ASSERT_TRUE (pb.has_winner ());
    ASSERT_TRUE (pb.has_winner_statement ());
    EXPECT_EQ (pb.winner (), winner);
  }

};

TEST_F (FullGameTests, PositionsNotSet)
{
  EXPECT_FALSE (ProcessAuto ());
  EXPECT_EQ (state->TurnCount (), 1);

  SetupPositions ();

  EXPECT_TRUE (ProcessAuto ());
  EXPECT_EQ (state->TurnCount (), 4);
}

TEST_F (FullGameTests, PrematureReveal)
{
  SetupPositions ();
  ProcessAuto ();

  if (state->WhoseTurn () == 1)
    {
      ProcessMove (GetCurrentChannel ().GetShotMove (Coord (0)));
      ProcessAuto ();
    }
  ASSERT_EQ (state->WhoseTurn (), 0);

  ProcessMove (GetCurrentChannel ().GetPositionRevealMove ());
  ProcessAuto ();

  LOG (INFO) << "Final state has turn count: " << state->TurnCount ();
  ExpectWinner (1);
}

TEST_F (FullGameTests, WithShots)
{
  SetupPositions ();
  ProcessAuto ();

  int nextTarget[] = {0, 0};
  while (state->WhoseTurn () != xaya::ParsedBoardState::NO_TURN)
    {
      const Coord target(nextTarget[state->WhoseTurn ()]++);
      ProcessMove (GetCurrentChannel ().GetShotMove (target));
      ProcessAuto ();
    }

  LOG (INFO) << "Final state has turn count: " << state->TurnCount ();
  ExpectWinner (0);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
