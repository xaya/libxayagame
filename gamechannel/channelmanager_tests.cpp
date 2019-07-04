// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelmanager.hpp"

#include "stateproof.hpp"
#include "testgame.hpp"

#include <xayautil/hash.hpp>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using testing::_;
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

} // anonymous namespace

class ChannelManagerTests : public TestGameFixture
{

protected:

  const uint256 channelId = SHA256::Hash ("channel id");
  proto::ChannelMetadata meta;

  ChannelManager cm;
  MockMoveSender onChain;

  ChannelManagerTests ()
    : cm(game.rules, rpcClient, channelId, "player")
  {
    cm.SetMoveSender (onChain);

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

} // anonymous namespace
} // namespace xaya
