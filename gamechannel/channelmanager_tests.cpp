// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelmanager_tests.hpp"

#include "protoutils.hpp"
#include "stateproof.hpp"

#include "proto/broadcast.pb.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using testing::_;
using testing::Return;
using testing::Truly;

namespace xaya
{

/* ************************************************************************** */

proto::StateProof
ValidProof (const std::string& state)
{
  proto::StateProof res;
  auto* is = res.mutable_initial_state ();
  is->set_data (state);
  is->add_signatures ("sgn");
  is->add_signatures ("other sgn");

  return res;
}

ChannelManagerTestFixture::ChannelManagerTestFixture ()
  : cm(game.rules, game.channel,
       verifier, signer,
       channelId, "player"),
    onChain("game id", cm.GetChannelId (), "player", txSender, game.channel)
{
  CHECK (TextFormat::ParseFromString (R"(
    participants:
      {
        name: "player"
        address: "my addr"
      }
    participants:
      {
        name: "other"
        address: "not my addr"
      }
  )", &meta));

  verifier.SetValid ("sgn", "my addr");
  verifier.SetValid ("other sgn", "not my addr");

  signer.SetAddress ("my addr");
  EXPECT_CALL (signer, SignMessage (_)).WillRepeatedly (Return ("sgn"));

  cm.SetMoveSender (onChain);
}

void
ChannelManagerTestFixture::ProcessOnChain (const BoardState& reinitState,
                                           const proto::StateProof& proof,
                                           const unsigned dispHeight)
{
  cm.ProcessOnChain (blockHash, height, meta, reinitState, proof, dispHeight);
}

void
ChannelManagerTestFixture::ProcessOnChainNonExistant ()
{
  cm.ProcessOnChainNonExistant (blockHash, height);
}

BoardState
ChannelManagerTestFixture::GetLatestState () const
{
  return UnverifiedProofEndState (cm.boardStates.GetStateProof ());
}

/* ************************************************************************** */

namespace
{

class ChannelManagerTests : public ChannelManagerTestFixture
{

protected:

  MockOffChainBroadcast offChain;

  ChannelManagerTests ()
    : offChain(cm.GetChannelId ())
  {
    cm.SetOffChainBroadcast (offChain);
  }

  /**
   * Expects exactly n dispute or resolution to be sent through the
   * mocked MoveSender, and checks that the associated state proof
   * matches that from GetBoardStates().
   *
   * Returns the txids that those moves will return in order.
   */
  std::vector<uint256>
  ExpectMoves (const unsigned n, const std::string& type)
  {
    const auto isOk = [this, type] (const std::string& val)
      {
        VLOG (1) << "on-chain move sent: " << val;

        const auto parsed = ParseJson (val);
        const auto& mv = parsed["g"]["game id"];

        if (!mv.isObject ())
          {
            VLOG (1) << "Not an object: " << mv;
            return false;
          }

        if (mv["type"].asString () != type)
          {
            VLOG (1) << "Mismatch in expected type, should be " << type;
            return false;
          }
        if (mv["id"].asString () != channelId.ToHex ())
          {
            VLOG (1) << "Mismatch in expected channel ID";
            return false;
          }

        proto::StateProof proof;
        if (!ProtoFromBase64 (mv["proof"].asString (), proof))
          {
            VLOG (1) << "Failed to parse proof from base64";
            return false;
          }

        const auto& expected = GetBoardStates ().GetStateProof ();
        if (!MessageDifferencer::Equals (proof, expected))
          {
            VLOG (1)
                << "State proof differs from expected proto"
                << "\nActual:\n" << proof.DebugString ()
                << "\nExpected:\n" << expected.DebugString ();
            return false;
          }

        return true;
      };

    return txSender.ExpectSuccess (n, "player", Truly (isOk));
  }

  /**
   * Expects a single move of the given type (as per ExpectMoves).
   */
  uint256
  ExpectMove (const std::string& type)
  {
    const auto txids = ExpectMoves (1, type);
    CHECK_EQ (txids.size (), 1);
    return txids[0];
  }

  /**
   * Expects exactly one off-chain broadcast to be sent with the latest state
   * proof, and verifies that the corresponding state matches the given one.
   */
  void
  ExpectOneBroadcast (const std::string& expectedState)
  {
    auto isOk = [this, expectedState] (const std::string& msg)
      {
        proto::BroadcastMessage pb;
        CHECK (pb.ParseFromString (msg));

        if (pb.reinit () != GetBoardStates ().GetReinitId ())
          return false;

        if (!GetBoardStates ().GetLatestState ().Equals (expectedState))
          return false;

        const auto& expectedProof = GetBoardStates ().GetStateProof ();
        return MessageDifferencer::Equals (pb.proof (), expectedProof);
      };
    EXPECT_CALL (offChain, SendMessage (Truly (isOk))).Times (1);
  }

};

TEST_F (ChannelManagerTests, ProcessOnChainNonExistant)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_TRUE (GetExists ());

  ProcessOnChainNonExistant ();
  EXPECT_FALSE (GetExists ());
}

/* ************************************************************************** */

using ProcessOnChainTests = ChannelManagerTests;

TEST_F (ProcessOnChainTests, Basic)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_TRUE (GetExists ());
  EXPECT_EQ (GetLatestState (), "10 5");
  EXPECT_EQ (GetDispute (), nullptr);
}

TEST_F (ProcessOnChainTests, Dispute)
{
  ProcessOnChain ("0 0", ValidProof ("11 5"), 10);
  EXPECT_NE (GetDispute (), nullptr);
  EXPECT_EQ (GetDispute ()->height, 10);
  EXPECT_EQ (GetDispute ()->turn, 1);
  EXPECT_EQ (GetDispute ()->count, 5);
  EXPECT_TRUE (GetDispute ()->pendingResolution.IsNull ());

  ProcessOnChain ("0 0", ValidProof ("12 6"), 0);
  EXPECT_EQ (GetDispute (), nullptr);
}

TEST_F (ProcessOnChainTests, TriggersResolutionn)
{
  ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
}

/* ************************************************************************** */

using ProcessOffChainTests = ChannelManagerTests;

TEST_F (ProcessOffChainTests, UpdatesState)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  EXPECT_EQ (GetLatestState (), "12 6");
}

TEST_F (ProcessOffChainTests, TriggersResolutionn)
{
  ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ProcessOffChainTests, WhenNotExists)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  ProcessOnChainNonExistant ();
  cm.ProcessOffChain ("", ValidProof ("20 10"));
  ProcessOnChain ("0 0", ValidProof ("15 7"), 0);
  EXPECT_EQ (GetLatestState (), "20 10");
}

/* ************************************************************************** */

using ProcessLocalMoveTests = ChannelManagerTests;

TEST_F (ProcessLocalMoveTests, WhenNotExists)
{
  ProcessOnChainNonExistant ();
  cm.ProcessLocalMove ("1");
  EXPECT_FALSE (GetExists ());
}

TEST_F (ProcessLocalMoveTests, InvalidUpdate)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessLocalMove ("invalid move");
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (ProcessLocalMoveTests, NotMyTurn)
{
  ProcessOnChain ("0 0", ValidProof ("11 5"), 0);
  cm.ProcessLocalMove ("1");
  EXPECT_EQ (GetLatestState (), "11 5");
}

TEST_F (ProcessLocalMoveTests, Valid)
{
  ExpectOneBroadcast ("11 6");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessLocalMove ("1");
  EXPECT_EQ (GetLatestState (), "11 6");
}

TEST_F (ProcessLocalMoveTests, TriggersResolution)
{
  ExpectOneBroadcast ("11 6");
  ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessLocalMove ("1");
}

/* ************************************************************************** */

using AutoMoveTests = ChannelManagerTests;

TEST_F (AutoMoveTests, OneMove)
{
  ExpectOneBroadcast ("20 6");
  ProcessOnChain ("0 0", ValidProof ("18 5"), 0);
  EXPECT_EQ (GetLatestState (), "20 6");
}

TEST_F (AutoMoveTests, TwoMoves)
{
  ExpectOneBroadcast ("30 7");
  ProcessOnChain ("0 0", ValidProof ("26 5"), 0);
  EXPECT_EQ (GetLatestState (), "30 7");
}

TEST_F (AutoMoveTests, NoTurnState)
{
  ProcessOnChain ("0 0", ValidProof ("108 5"), 0);
  EXPECT_EQ (GetLatestState (), "108 5");
}

TEST_F (AutoMoveTests, NotMyTurn)
{
  ProcessOnChain ("0 0", ValidProof ("37 5"), 0);
  EXPECT_EQ (GetLatestState (), "37 5");
}

TEST_F (AutoMoveTests, NoAutoMove)
{
  ProcessOnChain ("0 0", ValidProof ("44 5"), 0);
  EXPECT_EQ (GetLatestState (), "44 5");
}

TEST_F (AutoMoveTests, WithDisputeResolution)
{
  ExpectOneBroadcast ("50 6");
  ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("48 5"), 1);
  EXPECT_EQ (GetLatestState (), "50 6");
}

TEST_F (AutoMoveTests, ProcessOffChain)
{
  ExpectOneBroadcast ("20 9");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("18 8"));
  EXPECT_EQ (GetLatestState (), "20 9");
}

TEST_F (AutoMoveTests, ProcessLocalMove)
{
  ExpectOneBroadcast ("20 8");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessLocalMove ("6");
  EXPECT_EQ (GetLatestState (), "20 8");
}

/* ************************************************************************** */

using TriggerAutoMovesTests = ChannelManagerTests;

TEST_F (TriggerAutoMovesTests, NotOnChain)
{
  ProcessOnChainNonExistant ();

  /* This will just do nothing, but it also shouldn't CHECK-fail.  */
  cm.TriggerAutoMoves ();
}

TEST_F (TriggerAutoMovesTests, NoAutoMoves)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.TriggerAutoMoves ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (TriggerAutoMovesTests, SendsMoves)
{
  ExpectOneBroadcast ("10 6");

  game.channel.SetAutomovesEnabled (false);
  ProcessOnChain ("0 0", ValidProof ("8 5"), 0);
  EXPECT_EQ (GetLatestState (), "8 5");

  game.channel.SetAutomovesEnabled (true);
  cm.TriggerAutoMoves ();
  EXPECT_EQ (GetLatestState (), "10 6");
}

/* ************************************************************************** */

class MaybeOnChainMoveTests : public ChannelManagerTests
{

protected:

  /**
   * Adds an expectation for one of the "100" moves to be sent, as triggered
   * by the test game during MaybeOnChainMove.
   */
  void
  ExpectOnChainMove ()
  {
    const std::string expectedVal = R"({"g":{"game id":"100"}})";
    txSender.ExpectSuccess ("player", expectedVal);
  }

};

TEST_F (MaybeOnChainMoveTests, OnChain)
{
  ExpectOnChainMove ();
  ProcessOnChain ("0 0", ValidProof ("100 2"), 0);
}

TEST_F (MaybeOnChainMoveTests, OffChain)
{
  ExpectOnChainMove ();
  ProcessOnChain ("0 0", ValidProof ("55 2"), 0);
  cm.ProcessOffChain ("", ValidProof ("100 3"));
}

TEST_F (MaybeOnChainMoveTests, LocalMove)
{
  ExpectOneBroadcast ("100 3");
  ExpectOnChainMove ();
  ProcessOnChain ("0 0", ValidProof ("50 2"), 0);
  cm.ProcessLocalMove ("50");
}

TEST_F (MaybeOnChainMoveTests, AutoMoves)
{
  ExpectOneBroadcast ("100 4");
  ExpectOnChainMove ();
  ProcessOnChain ("0 0", ValidProof ("96 2"), 0);
}

TEST_F (MaybeOnChainMoveTests, NoOnChainMove)
{
  ProcessOnChain ("0 0", ValidProof ("110 2"), 0);
}

/* ************************************************************************** */

using ResolveDisputeTests = ChannelManagerTests;

TEST_F (ResolveDisputeTests, SendsResolution)
{
  ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, ChannelDoesNotExist)
{
  /* No moves are expected.  */
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  ProcessOnChainNonExistant ();
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, AlreadyPending)
{
  ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  cm.ProcessOffChain ("", ValidProof ("14 8"));
}

TEST_F (ResolveDisputeTests, OtherPlayer)
{
  /* No moves are expected.  */
  ProcessOnChain ("0 0", ValidProof ("11 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, NoBetterTurn)
{
  /* No moves are expected.  */
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 5"));
}

TEST_F (ResolveDisputeTests, RetryAfterBlock)
{
  ExpectMoves (2, "resolution");

  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));

  /* The previous resolution is still pending, so this will do nothing.  */
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("14 8"));

  /* Mark it as confirmed.  The ProcessOnChain will notice that, and the
     subsequent ProcessOffChain will then retry.  */
  txSender.ClearMempool ();
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("14 8"));
}

/* ************************************************************************** */

using PutStateOnChainTests = ChannelManagerTests;

TEST_F (PutStateOnChainTests, Successful)
{
  const auto txid = ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  EXPECT_EQ (cm.PutStateOnChain (), txid);
}

TEST_F (PutStateOnChainTests, ChannelDoesNotExist)
{
  /* No moves are expected.  */
  ProcessOnChainNonExistant ();
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  EXPECT_TRUE (cm.PutStateOnChain ().IsNull ());
}

TEST_F (PutStateOnChainTests, BestStateAlreadyOnChain)
{
  /* No moves are expected.  */
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("12 5"));
  EXPECT_TRUE (cm.PutStateOnChain ().IsNull ());
}

TEST_F (PutStateOnChainTests, MultipleUpdates)
{
  const auto txids = ExpectMoves (2, "resolution");
  ASSERT_EQ (txids.size (), 2);

  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);

  cm.ProcessOffChain ("", ValidProof ("12 6"));
  EXPECT_EQ (cm.PutStateOnChain (), txids[0]);

  cm.ProcessOffChain ("", ValidProof ("20 7"));
  EXPECT_EQ (cm.PutStateOnChain (), txids[1]);
}

/* ************************************************************************** */

using FileDisputeTests = ChannelManagerTests;

TEST_F (FileDisputeTests, Successful)
{
  const auto txid = ExpectMove ("dispute");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_EQ (cm.FileDispute (), txid);
}

TEST_F (FileDisputeTests, ChannelDoesNotExist)
{
  /* No moves are expected.  */
  ProcessOnChainNonExistant ();
  EXPECT_TRUE (cm.FileDispute ().IsNull ());
}

TEST_F (FileDisputeTests, HasOtherDispute)
{
  /* No moves are expected.  */
  ProcessOnChain ("0 0", ValidProof ("10 5"), 10);
  EXPECT_TRUE (cm.FileDispute ().IsNull ());
}

TEST_F (FileDisputeTests, AlreadyPending)
{
  const auto txid = ExpectMove ("dispute");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_EQ (cm.FileDispute (), txid);
  EXPECT_TRUE (cm.FileDispute ().IsNull ());
}

TEST_F (FileDisputeTests, RetryAfterBlock)
{
  const auto txids = ExpectMoves (2, "dispute");
  ASSERT_EQ (txids.size (), 2);

  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_EQ (cm.FileDispute (), txids[0]);

  /* The previous dispute is still pending.  */
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_TRUE (cm.FileDispute ().IsNull ());

  /* Mark it as not pending.  This will retry.  */
  txSender.ClearMempool ();
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_EQ (cm.FileDispute (), txids[1]);
}

/* ************************************************************************** */

using ChannelToJsonTests = ChannelManagerTests;

TEST_F (ChannelToJsonTests, Initial)
{
  auto expected = ParseJson (R"({
    "playername": "player",
    "existsonchain": false,
    "version": 1
  })");
  expected["id"] = channelId.ToHex ();

  EXPECT_EQ (cm.ToJson (), expected);
}

TEST_F (ChannelToJsonTests, NonExistant)
{
  auto expected = ParseJson (R"({
    "playername": "player",
    "existsonchain": false,
    "height": 42,
    "version": 2
  })");
  expected["id"] = channelId.ToHex ();
  expected["blockhash"] = blockHash.ToHex ();

  ProcessOnChainNonExistant ();
  EXPECT_EQ (cm.ToJson (), expected);
}

TEST_F (ChannelToJsonTests, CurrentState)
{
  ProcessOnChainNonExistant ();
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);

  auto actual = cm.ToJson ();
  EXPECT_EQ (actual["current"]["meta"]["participants"], ParseJson (R"([
    {"name": "player", "address": "my addr"},
    {"name": "other", "address": "not my addr"}
  ])"));
  EXPECT_EQ (actual["current"]["state"]["parsed"], ParseJson (R"({
    "number": 10,
    "count": 5
  })"));
  actual.removeMember ("current");

  auto expected = ParseJson (R"({
    "playername": "player",
    "existsonchain": true,
    "height": 42,
    "pending": {},
    "version": 3
  })");
  expected["id"] = channelId.ToHex ();
  expected["blockhash"] = blockHash.ToHex ();
  EXPECT_EQ (actual, expected);
}

TEST_F (ChannelToJsonTests, Dispute)
{
  ProcessOnChain ("0 0", ValidProof ("11 5"), 5);
  EXPECT_EQ (cm.ToJson ()["dispute"], ParseJson (R"({
    "height": 5,
    "whoseturn": 1,
    "canresolve": false
  })"));

  cm.ProcessOffChain ("", ValidProof ("20 6"));
  EXPECT_EQ (cm.ToJson ()["dispute"], ParseJson (R"({
    "height": 5,
    "whoseturn": 1,
    "canresolve": true
  })"));
}

TEST_F (ChannelToJsonTests, PendingPutStateOnChain)
{
  const auto txid = ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  cm.PutStateOnChain ();

  auto expected = Json::Value (Json::objectValue);
  expected["putstateonchain"] = txid.ToHex ();
  EXPECT_EQ (cm.ToJson ()["pending"], expected);
}

TEST_F (ChannelToJsonTests, PendingDispute)
{
  const auto txid = ExpectMove ("dispute");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  cm.FileDispute ();

  auto expected = Json::Value (Json::objectValue);
  expected["dispute"] = txid.ToHex ();
  EXPECT_EQ (cm.ToJson ()["pending"], expected);
}

TEST_F (ChannelToJsonTests, PendingResolution)
{
  const auto txid = ExpectMove ("resolution");
  ProcessOnChain ("0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));

  auto expected = Json::Value (Json::objectValue);
  expected["resolution"] = txid.ToHex ();
  EXPECT_EQ (cm.ToJson ()["pending"], expected);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
