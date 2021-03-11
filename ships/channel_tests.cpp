// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channel.hpp"

#include "board.hpp"
#include "proto/boardstate.pb.h"
#include "testutils.hpp"

#include <gamechannel/protoutils.hpp>
#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayagame/testutils.hpp>
#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>

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

class ChannelTests : public testing::Test
{

protected:

  const xaya::uint256 channelId = xaya::SHA256::Hash ("foo");

  /**
   * Two metadata instances, where "we" are either the first or second player.
   */
  xaya::proto::ChannelMetadata meta[2];

  ShipsBoardRules rules;
  ShipsChannel channel;

  ChannelTests ()
    : channel("player")
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
      reinit: "foo"
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
      reinit: "foo"
    )", &meta[1]));
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

  xaya::HttpRpcServer<xaya::MockXayaRpcServer> mockXayaServer;
  xaya::HttpRpcServer<xaya::MockXayaWalletRpcServer> mockXayaWallet;

  xaya::MoveSender sender;

  OnChainMoveTests ()
    : sender("xs", channelId, "player",
             mockXayaServer.GetClient (),
             mockXayaWallet.GetClient (),
             channel)
  {}

  /**
   * Parses a BoardState proto into a ParsedBoardState.  It uses the
   * metadata instance where the channel's user "player" is the first one.
   */
  std::unique_ptr<xaya::ParsedBoardState>
  ParseState (const proto::BoardState& pb)
  {
    return ChannelTests::ParseState (pb, meta[0]);
  }

  /**
   * Verifies that a given JSON object matches the expected move format
   * for the given key ("r", or "d"), channel ID and encoded data proto.
   * Note that both types of moves (disputes and resolutions) have the same
   * basic structure.
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

  /**
   * Verifies that a given JSON object matches the expected move format
   * for a loss declaration.
   */
  static bool
  IsExpectedLoss (const Json::Value& actual, const xaya::uint256& id,
                  const xaya::proto::ChannelMetadata& meta)
  {
    if (!actual.isObject ())
      return false;
    if (actual.size () != 1)
      return false;

    const auto& sub = actual["l"];
    if (!sub.isObject ())
      return false;
    if (sub.size () != 2)
      return false;

    if (!sub["id"].isString ())
      return false;
    if (sub["id"].asString () != id.ToHex ())
      return false;

    if (!sub["r"].isString ())
      return false;
    std::string reinit;
    if (!xaya::DecodeBase64 (sub["r"].asString (), reinit))
      return false;
    if (reinit != meta.reinit ())
      return false;

    return true;
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
  state.set_winner (0);

  channel.MaybeOnChainMove (*ParseState (state), sender);
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveSending)
{
  const auto isOk = [this] (const std::string& str)
    {
      const auto val = ParseJson (str);
      const auto& mv = val["g"]["xs"];
      return IsExpectedLoss (mv, channelId, meta[0]);
    };
  EXPECT_CALL (*mockXayaWallet, name_update ("p/player", Truly (isOk)))
      .WillOnce (Return (xaya::SHA256::Hash ("txid").ToHex ()));

  proto::BoardState state;
  state.set_winner (1);

  channel.MaybeOnChainMove (*ParseState (state), sender);
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveAlreadyPending)
{
  const auto txid = xaya::SHA256::Hash ("txid");

  const auto isOk = [this] (const std::string& str)
    {
      const auto val = ParseJson (str);
      const auto& mv = val["g"]["xs"];
      return IsExpectedLoss (mv, channelId, meta[0]);
    };
  EXPECT_CALL (*mockXayaWallet, name_update ("p/player", Truly (isOk)))
      .WillOnce (Return (txid.ToHex ()));

  Json::Value pendings(Json::arrayValue);
  Json::Value p(Json::objectValue);
  p["name"] = "p/player";
  p["txid"] = txid.ToHex ();
  pendings.append (p);
  EXPECT_CALL (*mockXayaServer, name_pending ())
      .WillOnce (Return (pendings));

  proto::BoardState state;
  state.set_winner (1);

  channel.MaybeOnChainMove (*ParseState (state), sender);
  channel.MaybeOnChainMove (*ParseState (state), sender);
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveNoLongerPending)
{
  const auto txid1 = xaya::SHA256::Hash ("txid 1");
  const auto txid2 = xaya::SHA256::Hash ("txid 2");

  const auto isOk = [this] (const std::string& str)
    {
      const auto val = ParseJson (str);
      const auto& mv = val["g"]["xs"];
      return IsExpectedLoss (mv, channelId, meta[0]);
    };
  EXPECT_CALL (*mockXayaWallet, name_update ("p/player", Truly (isOk)))
      .WillOnce (Return (txid1.ToHex ()))
      .WillOnce (Return (txid2.ToHex ()));

  EXPECT_CALL (*mockXayaServer, name_pending ())
      .WillOnce (Return (ParseJson ("[]")));

  proto::BoardState state;
  state.set_winner (1);

  channel.MaybeOnChainMove (*ParseState (state), sender);
  channel.MaybeOnChainMove (*ParseState (state), sender);
}

/* ************************************************************************** */

using PositionStoringTests = ChannelTests;

TEST_F (PositionStoringTests, SetPosition)
{
  ASSERT_FALSE (channel.IsPositionSet ());

  Grid pos;
  ASSERT_TRUE (pos.FromString (R"(
    xxxx....
    ........
    xxx.....
    ........
    xxx.....
    ........
    .x.x.x.x
    .x.x.x.x
  )"));

  channel.SetPosition (pos);
  EXPECT_TRUE (channel.IsPositionSet ());
}

TEST_F (PositionStoringTests, InvalidPosition)
{
  Grid pos;
  ASSERT_TRUE (pos.FromString (R"(
    xxxx....
    ........
    xxx.....
    ........
    xxx.....
    ........
    ........
    ........
  )"));

  channel.SetPosition (pos);
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
    CHECK (validPosition.FromString (R"(
      xxxx....
      ........
      xxx.....
      ........
      xxx.....
      ........
      .x.x.x.x
      .x.x.x.x
    )"));
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

  Grid allAndMore;
  CHECK (allAndMore.FromString (R"(
      xxxx...x
      .......x
      xxx....x
      .......x
      xxx....x
      ........
      .x.x.x.x
      .x.x.x.x
  )"));

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

/* ************************************************************************** */

/**
 * Tests that run a full game between two channel instances, just like
 * it would be done with automoves and real frontends.
 */
class FullGameTests : public ChannelTests
{

private:

  xaya::HttpRpcServer<xaya::MockXayaRpcServer> mockXayaServer;

  /** Indexable array of the channels.  */
  ShipsChannel* channels[2];

protected:

  ShipsChannel otherChannel;

  /** The current game state.  This is updated as moves are made.  */
  std::unique_ptr<xaya::ParsedBoardState> state;

  FullGameTests ()
    : otherChannel("other player")
  {
    channels[0] = &channel;
    channels[1] = &otherChannel;

    state = ParseState (InitialBoardState (), meta[0]);
  }

  /**
   * Sets up the positions of both channels.  They are chosen in such a way
   * that "channel" wins when guesses are made in increasing order (0, 1, ...),
   * i.e. the ships of "channel" are more towards the "higher coordinates".
   */
  void
  SetupPositions ()
  {
    Grid p;
    CHECK (p.FromString (R"(
      ........
      ........
      ........
      xx.xx.xx
      ........
      ..xx.xxx
      ........
      xxx.xxxx
    )"));
    channel.SetPosition (p);

    CHECK (p.FromString (R"(
      xx.xx.xx
      ........
      ..xx.xxx
      ........
      xxx.xxxx
      ........
      ........
      ........
    )"));
    otherChannel.SetPosition (p);

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
    CHECK (state->ApplyMove (mockXayaServer.GetClient (),
                             serialised, newState));

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
