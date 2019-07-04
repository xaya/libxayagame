// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rollingstate.hpp"

#include "stateproof.hpp"
#include "testgame.hpp"

#include <xayautil/hash.hpp>

#include <google/protobuf/text_format.h>

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;

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

class RollingStateTests : public TestGameFixture
{

protected:

  const uint256 channelId = SHA256::Hash ("channel id");
  proto::ChannelMetadata meta1;
  proto::ChannelMetadata meta2;

  RollingState state;

  RollingStateTests ()
    : state(game.rules, rpcClient, channelId)
  {
    meta1.set_reinit ("reinit 1");
    meta1.add_participants ()->set_address ("addr 0");
    meta1.add_participants ()->set_address ("addr 1");

    meta2.set_reinit ("reinit 2");
    meta2.add_participants ()->set_address ("addr 0");
    meta2.add_participants ()->set_address ("addr 2");

    ValidSignature ("sgn 0", "addr 0");
    ValidSignature ("sgn 1", "addr 1");
    ValidSignature ("sgn 2", "addr 2");
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
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )"));
  ExpectState ("13 5", "reinit 1");

  state.UpdateOnChain (meta2, "25 4", ParseStateProof (R"(
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
  ExpectState ("65 5", "reinit 2");

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
  ExpectState ("63 6", "reinit 1");

  state.UpdateOnChain (meta2, "25 4", ParseStateProof (R"(
    initial_state: { data: "25 4" }
    transitions:
      {
        move: "20"
        new_state:
          {
            data: "45 5"
            signatures: "sgn 2"
          }
      }
  )"));
  ExpectState ("65 5", "reinit 2");
}

TEST_F (RollingStateTests, UpdateWithMoveUnknownReinit)
{
  state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
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

  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )"));
  ExpectState ("13 5", "reinit 1");
}

TEST_F (RollingStateTests, UpdateWithMoveInvalidProof)
{
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
    initial_state: { data: "13 5" }
  )"));

  state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
    initial_state: { data: "50 6" }
  )"));

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

  state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
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
  )"));

  ExpectState ("63 6", "reinit 1");
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
  state.UpdateWithMove ("reinit 1", ParseStateProof (R"(
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
  )"));
  ExpectState ("25 4", "reinit 2");

  /* This on-chain update switches to "reinit 1" but is not fresher in turn
     count than the off-chain update from before.  */
  state.UpdateOnChain (meta1, "13 5", ParseStateProof (R"(
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
  )"));
  ExpectState ("60 7", "reinit 1");
}

} // anonymous namespace
} // namespace xaya
