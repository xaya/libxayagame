// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelmanager.hpp"

#include "stateproof.hpp"
#include "testgame.hpp"

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

namespace
{

/**
 * Constructs a state proof for the given state, signed by both players
 * (and thus valid).
 */
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

class MockMoveSender : public MoveSender
{

public:

  MockMoveSender ()
  {
    /* By default, we do not expect any calls.  Tests should explicitly
       overwrite these expectations as needed.  */
    EXPECT_CALL (*this, SendDispute (_)).Times (0);
    EXPECT_CALL (*this, SendResolution (_)).Times (0);
  }

  MOCK_METHOD1 (SendDispute, void (const proto::StateProof& proof));
  MOCK_METHOD1 (SendResolution, void (const proto::StateProof& proof));

};

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

} // anonymous namespace

class ChannelManagerTests : public TestGameFixture
{

protected:

  const uint256 channelId = SHA256::Hash ("channel id");
  proto::ChannelMetadata meta;

  ChannelManager cm;
  MockMoveSender onChain;
  MockOffChainBroadcast offChain;

  ChannelManagerTests ()
    : cm(game.rules, rpcClient, rpcWallet, channelId, "player")
  {
    cm.SetMoveSender (onChain);
    cm.SetOffChainBroadcast (offChain);

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

  /**
   * Extracts the latest state from boardStates.
   */
  BoardState
  GetLatestState () const
  {
    return UnverifiedProofEndState (cm.boardStates.GetStateProof ());
  }

  /**
   * Exposes the boardStates member of our ChannelManager to subtests.
   */
  const RollingState&
  GetBoardStates () const
  {
    return cm.boardStates;
  }

  /**
   * Exposes the exists member to subtests.
   */
  bool
  GetExists () const
  {
    return cm.exists;
  }

  /**
   * Exposes the dispute member to subtests.
   */
  const ChannelManager::DisputeData*
  GetDispute () const
  {
    return cm.dispute.get ();
  }

  /**
   * Expects exactly n calls to SendResolution to be made, with the state
   * proof from GetBoardStates().
   */
  void
  ExpectResolutions (const int n)
  {
    auto isOk = [this] (const proto::StateProof& p)
      {
        const auto& expected = GetBoardStates ().GetStateProof ();
        return MessageDifferencer::Equals (p, expected);
      };
    EXPECT_CALL (onChain, SendResolution (Truly (isOk))).Times (n);
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

namespace
{

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
  ExpectResolutions (1);
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
  ExpectResolutions (1);
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
  ExpectResolutions (1);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessLocalMove ("1");
}

/* ************************************************************************** */

using ResolveDisputeTests = ChannelManagerTests;

TEST_F (ResolveDisputeTests, SendsResolution)
{
  ExpectResolutions (1);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, ChannelDoesNotExist)
{
  ExpectResolutions (0);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOnChainNonExistant ();
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, AlreadyPending)
{
  ExpectResolutions (1);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  cm.ProcessOffChain ("", ValidProof ("14 8"));
}

TEST_F (ResolveDisputeTests, OtherPlayer)
{
  ExpectResolutions (0);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("11 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 6"));
}

TEST_F (ResolveDisputeTests, NoBetterTurn)
{
  ExpectResolutions (0);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 1);
  cm.ProcessOffChain ("", ValidProof ("12 5"));
}

/* ************************************************************************** */

class FileDisputeTests : public ChannelManagerTests
{

protected:

  /**
   * Expects exactly n calls to SendDispute to be made, with the state
   * proof from GetBoardStates().
   */
  void
  ExpectDisputes (const int n)
  {
    auto isOk = [this] (const proto::StateProof& p)
      {
        const auto& expected = GetBoardStates ().GetStateProof ();
        return MessageDifferencer::Equals (p, expected);
      };
    EXPECT_CALL (onChain, SendDispute (Truly (isOk))).Times (n);
  }

};

TEST_F (FileDisputeTests, Successful)
{
  ExpectDisputes (1);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, ChannelDoesNotExist)
{
  ExpectDisputes (0);
  cm.ProcessOnChainNonExistant ();
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, HasOtherDispute)
{
  ExpectDisputes (0);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 10);
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, AlreadyPending)
{
  ExpectDisputes (1);
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  cm.FileDispute ();
  cm.FileDispute ();
}

TEST_F (FileDisputeTests, RetryAfterBlock)
{
  ExpectDisputes (2);
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
    "existsonchain": false
  })");
  expected["id"] = channelId.ToHex ();

  cm.ProcessOnChainNonExistant ();
  EXPECT_EQ (cm.ToJson (), expected);
}

TEST_F (ChannelToJsonTests, CurrentState)
{
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
    "existsonchain": true
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
  CallWaitForChange ()
  {
    CHECK (waiter == nullptr);
    waiter = std::make_unique<std::thread> ([this] ()
      {
        LOG (INFO) << "Calling WaitForChange...";
        returnedJson = cm.WaitForChange ();
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

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
