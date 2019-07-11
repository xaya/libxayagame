// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelmanager_tests.hpp"

#include "protoutils.hpp"
#include "stateproof.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <thread>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using testing::_;
using testing::Return;
using testing::Throw;
using testing::Truly;

namespace xaya
{

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
  : cm(game.rules, rpcClient, rpcWallet, channelId, "player")
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

  ValidSignature ("sgn", "my addr");
  ValidSignature ("other sgn", "not my addr");

  EXPECT_CALL (mockXayaWallet, signmessage ("my addr", _))
      .WillRepeatedly (Return (EncodeBase64 ("sgn")));
  EXPECT_CALL (mockXayaWallet, signmessage ("not my addr", _))
      .WillRepeatedly (Throw (jsonrpc::JsonRpcException (-5)));
}

ChannelManagerTestFixture::~ChannelManagerTestFixture ()
{
  cm.StopUpdates ();
}

BoardState
ChannelManagerTestFixture::GetLatestState () const
{
  return UnverifiedProofEndState (cm.boardStates.GetStateProof ());
}

namespace
{

class MockOffChainBroadcast : public OffChainBroadcast
{

public:

  MockOffChainBroadcast ()
  {
    /* Expect no calls by default.  */
    EXPECT_CALL (*this, SendNewState (_, _)).Times (0);
  }

  MOCK_METHOD2 (SendNewState, void (const std::string& reinitId,
                                    const proto::StateProof& proof));

};

class ChannelManagerTests : public ChannelManagerTestFixture
{

protected:

  MoveSender onChain;
  MockOffChainBroadcast offChain;

  ChannelManagerTests ()
    : onChain("game id", channelId, "player", rpcWallet, game.channel)
  {
    cm.SetMoveSender (onChain);
    cm.SetOffChainBroadcast (offChain);
  }

  /**
   * Expects exactly n disputes or resolutions to be sent through the
   * wallet with name_update's, and checks that the associated state proof
   * matches that from GetBoardStates().
   */
  void
  ExpectMoves (const int n, const std::string& type)
  {
    auto isOk = [this, type] (const std::string& val)
      {
        VLOG (1) << "name_update sent: " << val;

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

    EXPECT_CALL (mockXayaWallet, name_update ("p/player", Truly (isOk)))
        .Times (n)
        .WillRepeatedly (Return (SHA256::Hash ("txid").ToHex ()));
  }

  /**
   * Expects exactly one off-chain broadcast to be sent with the latest state.
   */
  void
  ExpectOneBroadcast ()
  {
    auto isLatestReinit = [this] (const std::string& reinit)
      {
        return reinit == GetBoardStates ().GetReinitId ();
      };
    auto isLatestProof = [this] (const proto::StateProof& p)
      {
        const auto& expected = GetBoardStates ().GetStateProof ();
        return MessageDifferencer::Equals (p, expected);
      };
    EXPECT_CALL (offChain,
                 SendNewState (Truly (isLatestReinit), Truly (isLatestProof)))
        .Times (1);
  }

};

TEST_F (ChannelManagerTests, ProcessOnChainNonExistant)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  EXPECT_TRUE (GetExists ());

  cm.ProcessOnChainNonExistant ();
  EXPECT_FALSE (GetExists ());
}

/* ************************************************************************** */

using ProcessOnChainTests = ChannelManagerTests;

TEST_F (ProcessOnChainTests, Basic)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  EXPECT_TRUE (GetExists ());
  EXPECT_EQ (GetLatestState (), "10 5");
  EXPECT_EQ (GetDispute (), nullptr);
}

TEST_F (ProcessOnChainTests, Dispute)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("11 5"), 10);
  EXPECT_NE (GetDispute (), nullptr);
  EXPECT_EQ (GetDispute ()->height, 10);
  EXPECT_EQ (GetDispute ()->turn, 1);
  EXPECT_EQ (GetDispute ()->count, 5);
  EXPECT_EQ (GetDispute ()->pendingResolution, false);

  cm.ProcessOnChain (meta, "0 0", ValidProof ("12 6"), 0);
  EXPECT_EQ (GetDispute (), nullptr);
}

TEST_F (ProcessOnChainTests, TriggersResolutionn)
{
  ExpectMoves (1, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
}

/* ************************************************************************** */

using ProcessOffChainTests = ChannelManagerTests;

TEST_F (ProcessOffChainTests, UpdatesState)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  EXPECT_EQ (GetLatestState (), "12 6");
}

TEST_F (ProcessOffChainTests, TriggersResolutionn)
{
  ExpectMoves (1, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ProcessOffChainTests, WhenNotExists)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.ProcessOnChainNonExistant ();
  cm.ProcessOffChain ("", ValidProof ("20 10"));
  cm.ProcessOnChain (meta, "0 0", ValidProof ("15 7"), 0);
  EXPECT_EQ (GetLatestState (), "20 10");
}

/* ************************************************************************** */

using ProcessLocalMoveTests = ChannelManagerTests;

TEST_F (ProcessLocalMoveTests, WhenNotExists)
{
  cm.ProcessOnChainNonExistant ();
  cm.ProcessLocalMove ("1");
  EXPECT_FALSE (GetExists ());
}

TEST_F (ProcessLocalMoveTests, InvalidUpdate)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.ProcessLocalMove ("invalid move");
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (ProcessLocalMoveTests, NotMyTurn)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("11 5"), 0);
  cm.ProcessLocalMove ("1");
  EXPECT_EQ (GetLatestState (), "11 5");
}

TEST_F (ProcessLocalMoveTests, Valid)
{
  ExpectOneBroadcast ();
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.ProcessLocalMove ("1");
  EXPECT_EQ (GetLatestState (), "11 6");
}

TEST_F (ProcessLocalMoveTests, TriggersResolution)
{
  ExpectOneBroadcast ();
  ExpectMoves (1, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessLocalMove ("1");
}

/* ************************************************************************** */

using ResolveDisputeTests = ChannelManagerTests;

TEST_F (ResolveDisputeTests, SendsResolution)
{
  ExpectMoves (1, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, ChannelDoesNotExist)
{
  ExpectMoves (0, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOnChainNonExistant ();
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, AlreadyPending)
{
  ExpectMoves (1, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  cm.ProcessOffChain ("", ValidProof ("14 8"));
}

TEST_F (ResolveDisputeTests, OtherPlayer)
{
  ExpectMoves (0, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("11 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, NoBetterTurn)
{
  ExpectMoves (0, "resolution");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 5"));
}

/* ************************************************************************** */

using FileDisputeTests = ChannelManagerTests;

TEST_F (FileDisputeTests, Successful)
{
  ExpectMoves (1, "dispute");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, ChannelDoesNotExist)
{
  ExpectMoves (0, "dispute");
  cm.ProcessOnChainNonExistant ();
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, HasOtherDispute)
{
  ExpectMoves (0, "dispute");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 10);
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, AlreadyPending)
{
  ExpectMoves (1, "dispute");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.FileDispute ();
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, RetryAfterBlock)
{
  ExpectMoves (2, "dispute");
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.FileDispute ();
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.FileDispute ();
}

/* ************************************************************************** */

using ChannelToJsonTests = ChannelManagerTests;

TEST_F (ChannelToJsonTests, NonExistant)
{
  auto expected = ParseJson (R"({
    "playername": "player",
    "existsonchain": false,
    "version": 2
  })");
  expected["id"] = channelId.ToHex ();

  cm.ProcessOnChainNonExistant ();
  EXPECT_EQ (cm.ToJson (), expected);
}

TEST_F (ChannelToJsonTests, CurrentState)
{
  cm.ProcessOnChainNonExistant ();
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);

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
    "version": 3
  })");
  expected["id"] = channelId.ToHex ();
  EXPECT_EQ (actual, expected);
}

TEST_F (ChannelToJsonTests, Dispute)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("11 5"), 5);
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

/* ************************************************************************** */

class WaitForChangeTests : public ChannelManagerTests
{

private:

  /** The thread that is used to call WaitForChange.  */
  std::unique_ptr<std::thread> waiter;

  /** The JSON value returned from WaitForChange.  */
  Json::Value returnedJson;

protected:

  /**
   * Calls WaitForChange on a newly started thread.
   */
  void
  CallWaitForChange (int known = ChannelManager::WAITFORCHANGE_ALWAYS_BLOCK)
  {
    CHECK (waiter == nullptr);
    waiter = std::make_unique<std::thread> ([this, known] ()
      {
        LOG (INFO) << "Calling WaitForChange...";
        returnedJson = cm.WaitForChange (known);
        LOG (INFO) << "WaitForChange returned";
      });

    /* Make sure the thread had time to start and make the call.  */
    SleepSome ();
  }

  /**
   * Waits for the waiter thread to return and checks that the JSON value
   * from it matches the then-correct ToJson output.
   */
  void
  JoinWaiter ()
  {
    CHECK (waiter != nullptr);
    LOG (INFO) << "Joining the waiter thread...";
    waiter->join ();
    LOG (INFO) << "Waiter thread finished";
    waiter.reset ();
    ASSERT_EQ (returnedJson, cm.ToJson ());
  }

};

TEST_F (WaitForChangeTests, OnChain)
{
  CallWaitForChange ();
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, OnChainNonExistant)
{
  CallWaitForChange ();
  cm.ProcessOnChainNonExistant ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, OffChain)
{
  CallWaitForChange ();
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, LocalMove)
{
  ExpectOneBroadcast ();
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  CallWaitForChange ();
  cm.ProcessLocalMove ("1");
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, WhenStopped)
{
  cm.StopUpdates ();
  CallWaitForChange ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, StopNotifies)
{
  CallWaitForChange ();
  cm.StopUpdates ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, OutdatedKnownVersion)
{
  const int known = cm.ToJson ()["version"].asInt ();
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  CallWaitForChange (known);
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, UpToDateKnownVersion)
{
  const int known = cm.ToJson ()["version"].asInt ();
  CallWaitForChange (known);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  JoinWaiter ();
}

/* ************************************************************************** */

using StopUpdatesTests = ChannelManagerTests;

TEST_F (StopUpdatesTests, OnChain)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.StopUpdates ();
  cm.ProcessOnChain (meta, "0 0", ValidProof ("20 6"), 0);
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (StopUpdatesTests, OnChainNonExistant)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.StopUpdates ();
  cm.ProcessOnChainNonExistant ();
  EXPECT_TRUE (GetExists ());
}

TEST_F (StopUpdatesTests, OffChain)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.StopUpdates ();
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (StopUpdatesTests, LocalMove)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.StopUpdates ();
  cm.ProcessLocalMove ("1");
  EXPECT_EQ (GetLatestState (), "10 5");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
