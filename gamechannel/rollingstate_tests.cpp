// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rollingstate.hpp"

#include "stateproof.hpp"
#include "testgame.hpp"

#include <xayautil/hash.hpp>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;

/**
 * Parses a StateProof from text proto.
 */
proto::StateProof
ParseStateProof (const std::string& str)
{
  proto::StateProof res;
  CHECK (TextFormat::ParseFromString (str, &res));

  return res;
}

/* ************************************************************************** */

class StateUpdateQueueTests : public testing::Test
{

protected:

  /**
   * Constructs a test state proof based just on an integer.
   */
  static proto::StateProof
  TestProof (const int i)
  {
    std::ostringstream str;
    str << i;

    proto::StateProof res;
    res.mutable_initial_state ()->set_data (str.str ());
    return res;
  }

  /**
   * Checks that the returned list of state proofs equals the test
   * proofs for the given numbers.
   */
  static void
  ExpectTestProofs (const std::deque<proto::StateProof>& actual,
                    const std::vector<int>& expected)
  {
    ASSERT_EQ (actual.size (), expected.size ());
    for (unsigned i = 0; i < actual.size (); ++i)
      ASSERT_TRUE (MessageDifferencer::Equals (actual[i],
                                               TestProof (expected[i])));
  }

};

TEST_F (StateUpdateQueueTests, BasicAddingAndExtracting)
{
  StateUpdateQueue q(3);

  EXPECT_EQ (q.GetTotalSize (), 0);
  ExpectTestProofs (q.ExtractQueue ("foo"), {});

  q.Insert ("foo", TestProof (1));
  q.Insert ("bar", TestProof (2));
  q.Insert ("foo", TestProof (3));
  EXPECT_EQ (q.GetTotalSize (), 3);
  ExpectTestProofs (q.ExtractQueue ("foo"), {1, 3});
  EXPECT_EQ (q.GetTotalSize (), 1);

  q.Insert ("foo", TestProof (4));
  EXPECT_EQ (q.GetTotalSize (), 2);
  ExpectTestProofs (q.ExtractQueue ("foo"), {4});
  ExpectTestProofs (q.ExtractQueue ("bar"), {2});
  EXPECT_EQ (q.GetTotalSize (), 0);
}

TEST_F (StateUpdateQueueTests, OverflowClearsOtherReinits)
{
  StateUpdateQueue q(4);

  q.Insert ("foo", TestProof (1));
  q.Insert ("bar", TestProof (2));
  q.Insert ("bar", TestProof (3));
  q.Insert ("baz", TestProof (4));
  EXPECT_EQ (q.GetTotalSize (), 4);

  q.Insert ("baz", TestProof (5));
  EXPECT_EQ (q.GetTotalSize (), 2);
  ExpectTestProofs (q.ExtractQueue ("foo"), {});
  ExpectTestProofs (q.ExtractQueue ("bar"), {});
  ExpectTestProofs (q.ExtractQueue ("baz"), {4, 5});
}

TEST_F (StateUpdateQueueTests, OverflowRemovesOldestEntries)
{
  StateUpdateQueue q(3);

  q.Insert ("foo", TestProof (1));
  q.Insert ("bar", TestProof (2));
  q.Insert ("baz", TestProof (3));
  EXPECT_EQ (q.GetTotalSize (), 3);

  q.Insert ("baz", TestProof (4));
  q.Insert ("baz", TestProof (5));
  q.Insert ("baz", TestProof (6));
  EXPECT_EQ (q.GetTotalSize (), 3);
  ExpectTestProofs (q.ExtractQueue ("foo"), {});
  ExpectTestProofs (q.ExtractQueue ("bar"), {});
  ExpectTestProofs (q.ExtractQueue ("baz"), {4, 5, 6});
}

/* ************************************************************************** */

class RollingStateTests : public TestGameFixture
{

protected:

  const std::string gameId = "game id";
  const uint256 channelId = SHA256::Hash ("channel id");
  proto::ChannelMetadata meta1;
  proto::ChannelMetadata meta2;

  RollingState state;

  RollingStateTests ()
    : state(game.rules, verifier, gameId, channelId)
  {
    meta1.set_reinit ("reinit 1");
    meta1.add_participants ()->set_address ("addr 0");
    meta1.add_participants ()->set_address ("addr 1");

    meta2.set_reinit ("reinit 2");
    meta2.add_participants ()->set_address ("addr 0");
    meta2.add_participants ()->set_address ("addr 2");

    verifier.SetValid ("sgn 0", "addr 0");
    verifier.SetValid ("sgn 1", "addr 1");
    verifier.SetValid ("sgn 2", "addr 2");
  }

  /**
   * Expects that the latest state and associated reinit ID for the rolling
   * state matches the given data.  This checks both GetLatestState and
   * GetStateProof against the expected state.
   */
  void
  ExpectState (const BoardState& expectedState, const std::string& reinitId)
  {
    EXPECT_EQ (state.GetReinitId (), reinitId);
    EXPECT_TRUE (state.GetLatestState ().Equals (expectedState));
    EXPECT_EQ (UnverifiedProofEndState (state.GetStateProof ()), expectedState);
  }

};

TEST_F (RollingStateTests, OnChainUpdate)
{
  EXPECT_TRUE (state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )")));
  ExpectState ("13 5", "reinit 1");
  EXPECT_EQ (state.GetOnChainTurnCount (), 5);

  EXPECT_TRUE (state.UpdateOnChain (meta2, "25 4", ParseStateProof (R"(
    initial_state: { data: "25 4" }
    transitions:
      {
        move: "40"
        new_state:
          {
            data: "65 5"
            signatures: "sgn 2"
          }
      }
  )")));
  ExpectState ("65 5", "reinit 2");
  EXPECT_EQ (state.GetOnChainTurnCount (), 5);

  EXPECT_TRUE (state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
    transitions:
      {
        move: "50"
        new_state:
          {
            data: "63 6"
            signatures: "sgn 1"
          }
      }
  )")));
  ExpectState ("63 6", "reinit 1");
  EXPECT_EQ (state.GetOnChainTurnCount (), 6);

  /* This provides a state proof that is older than the best known state,
     but it does change the current reinit ID.  */
  EXPECT_TRUE (state.UpdateOnChain (meta2, "25 4", ParseStateProof (R"(
    initial_state:
      {
        data: "45 5"
        signatures: "sgn 0"
        signatures: "sgn 2"
      }
  )")));
  ExpectState ("65 5", "reinit 2");
  EXPECT_EQ (state.GetOnChainTurnCount (), 5);

  /* This is an older state and does not change the reinit ID.  */
  EXPECT_FALSE (state.UpdateOnChain (meta2, "25 4", ParseStateProof (R"(
    initial_state:
      {
        data: "45 5"
        signatures: "sgn 0"
        signatures: "sgn 2"
      }
  )")));
  ExpectState ("65 5", "reinit 2");
  EXPECT_EQ (state.GetOnChainTurnCount (), 5);
}

TEST_F (RollingStateTests, UpdateWithMoveUnknownReinit)
{
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )"));
  ExpectState ("13 5", "reinit 1");

  /* This update gets queued initially, and then applied once
     the on-chain update is there.  */
  state.UpdateWithMove ("reinit 2", ParseStateProof (R"(
    initial_state: { data: "25 4" }
    transitions:
      {
        move: "40"
        new_state:
          {
            data: "65 5"
            signatures: "sgn 2"
          }
      }
  )"));
  ExpectState ("13 5", "reinit 1");

  state.UpdateOnChain (meta2, "25 4", ParseStateProof (R"(
    initial_state: { data: "25 4" }
  )"));
  ExpectState ("65 5", "reinit 2");
}

TEST_F (RollingStateTests, UpdateWithMoveInvalidProof)
{
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )"));

  EXPECT_FALSE (state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
    initial_state: { data: "50 6" }
  )")));

  ExpectState ("13 5", "reinit 1");
}

TEST_F (RollingStateTests, UpdateWithMoveInvalidProtoVersion)
{
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )"));

  EXPECT_FALSE (state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
    initial_state:
      {
        data: "50 6"
        signatures: "sgn 0"
        signatures: "sgn 1"
        for_testing_version: "foo"
      }
  )")));

  ExpectState ("13 5", "reinit 1");
}

TEST_F (RollingStateTests, UpdateWithMoveNotFresher)
{
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
    transitions:
      {
        move: "50"
        new_state:
          {
            data: "63 6"
            signatures: "sgn 1"
          }
      }
  )"));

  EXPECT_FALSE (state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
    initial_state: { data: "13 5" }
    transitions:
      {
        move: "20"
        new_state:
          {
            data: "33 6"
            signatures: "sgn 1"
          }
      }
  )")));

  ExpectState ("63 6", "reinit 1");
  EXPECT_EQ (state.GetOnChainTurnCount (), 6);
}

TEST_F (RollingStateTests, UpdateWithMoveSuccessful)
{
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )"));
  state.UpdateOnChain (meta2, "25 4", ParseStateProof (R"(
    initial_state: { data: "25 4" }
  )"));
  ExpectState ("25 4", "reinit 2");

  /* Successful off-chain update, although to a reinitialisation that is not
     the current one.  */
  EXPECT_FALSE (state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
    initial_state: { data: "13 5" }
    transitions:
      {
        move: "37"
        new_state:
          {
            data: "50 6"
            signatures: "sgn 1"
          }
      }
    transitions:
      {
        move: "10"
        new_state:
          {
            data: "60 7"
            signatures: "sgn 0"
          }
      }
  )")));
  ExpectState ("25 4", "reinit 2");
  EXPECT_EQ (state.GetOnChainTurnCount (), 4);

  /* This on-chain update switches to "reinit 1" but is not fresher in turn
     count than the off-chain update from before.  */
  EXPECT_TRUE (state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
    transitions:
      {
        move: "10"
        new_state:
          {
            data: "23 6"
            signatures: "sgn 1"
          }
      }
  )")));
  ExpectState ("60 7", "reinit 1");
  EXPECT_EQ (state.GetOnChainTurnCount (), 6);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
