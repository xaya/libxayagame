// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stateproof.hpp"

#include "testgame.hpp"

#include <google/protobuf/text_format.h>

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;

class GeneralStateProofTests : public TestGameFixture
{

protected:

  proto::ChannelMetadata meta;

  GeneralStateProofTests ()
  {
    meta.add_participants ()->set_address ("addr0");
    meta.add_participants ()->set_address ("addr1");

    ValidSignature ("sgn0", "addr0");
    ValidSignature ("sgn1", "addr1");
    ValidSignature ("sgn42", "addr42");
  }

};

/* ************************************************************************** */

class StateTransitionTests : public GeneralStateProofTests
{

protected:

  /**
   * Calls VerifyStateTransition based on the given state and the
   * transition proto loaded from text format.
   */
  bool
  VerifyTransition (const BoardState& oldState, const std::string& transition)
  {
    proto::StateTransition proto;
    CHECK (TextFormat::ParseFromString (transition, &proto));

    return VerifyStateTransition (rpcClient, game.rules, meta, oldState, proto);
  }

};

TEST_F (StateTransitionTests, NoTurnState)
{
  EXPECT_FALSE (VerifyTransition ("100 1", R"(
    move: "1",
    new_state:
      {
        data: "101 2"
        signatures: "sgn0"
      }
  )"));
}

TEST_F (StateTransitionTests, InvalidOldState)
{
  EXPECT_FALSE (VerifyTransition ("invalid", R"(
    move: "0",
    new_state:
      {
        data: "0 2"
        signatures: "sgn0"
      }
  )"));
}

TEST_F (StateTransitionTests, InvalidClaimedNewState)
{
  EXPECT_FALSE (VerifyTransition ("10 1", R"(
    move: "0",
    new_state:
      {
        data: "invalid"
        signatures: "sgn0"
      }
  )"));
}

TEST_F (StateTransitionTests, InvalidMove)
{
  EXPECT_FALSE (VerifyTransition ("10 1", R"(
    move: "0",
    new_state:
      {
        data: "10 2"
        signatures: "sgn0"
      }
  )"));
}

TEST_F (StateTransitionTests, NewStateMismatch)
{
  EXPECT_FALSE (VerifyTransition ("10 1", R"(
    move: "1",
    new_state:
      {
        data: "11 5"
        signatures: "sgn0"
      }
  )"));
}

TEST_F (StateTransitionTests, InvalidSignature)
{
  EXPECT_FALSE (VerifyTransition ("10 1", R"(
    move: "1",
    new_state:
      {
        data: "11 2"
        signatures: "sgn1"
        signatures: "sgn42"
      }
  )"));
}

TEST_F (StateTransitionTests, Valid)
{
  ExpectSignature (" 11 2 ", "signed by zero", "addr0");

  EXPECT_TRUE (VerifyTransition ("10 1", R"(
    move: "1",
    new_state:
      {
        data: " 11 2 "
        signatures: "signed by zero"
        signatures: "sgn42"
      }
  )"));
}

/* ************************************************************************** */

class StateProofTests : public GeneralStateProofTests
{

protected:

  BoardState endState;

  /**
   * Calls VerifyStateProof based on the given on-chain state and the
   * proof proto loaded from text format.
   */
  bool
  VerifyProof (const BoardState& chainState, const std::string& proof)
  {
    proto::StateProof proto;
    CHECK (TextFormat::ParseFromString (proof, &proto));

    return VerifyStateProof (rpcClient, game.rules, meta, chainState, proto,
                             endState);
  }

};

TEST_F (StateProofTests, InvalidStates)
{
  EXPECT_FALSE (VerifyProof (" 42 5 ", R"(
    initial_state:
      {
        data: "invalid"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));

  EXPECT_FALSE (VerifyProof (" 42 5 ", R"(
    initial_state:
      {
        data: "invalid"
        signatures: "sgn1"
      }
    transitions:
      {
        move: "2"
        new_state:
          {
            data: "44 6"
            signatures: "sgn0"
          }
      }
  )"));

  EXPECT_FALSE (VerifyProof (" 42 5 ", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn1"
      }
    transitions:
      {
        move: "2"
        new_state:
          {
            data: "invalid"
            signatures: "sgn0"
          }
      }
  )"));
}

TEST_F (StateProofTests, OnlyInitialOnChain)
{
  ASSERT_TRUE (VerifyProof (" 42 5 ", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn42"
      }
  )"));
  EXPECT_EQ (endState, "42 5");
}

TEST_F (StateProofTests, OnlyInitialSigned)
{
  ExpectSignature ("42 5", "signature 0", "addr0");
  ExpectSignature ("42 5", "signature 1", "addr1");

  ASSERT_TRUE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "signature 0"
        signatures: "signature 1"
      }
  )"));
  EXPECT_EQ (endState, "42 5");
}

TEST_F (StateProofTests, OnlyInitialNotSigned)
{
  EXPECT_FALSE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn42"
      }
  )"));
}

TEST_F (StateProofTests, InvalidTransition)
{
  EXPECT_FALSE (VerifyProof ("42 1", R"(
    initial_state:
      {
        data: "42 1"
      }
    transitions:
      {
        move: "0"
        new_state:
          {
            data: "42 2"
          }
      }
  )"));

  EXPECT_FALSE (VerifyProof ("42 1", R"(
    initial_state:
      {
        data: "42 1"
      }
    transitions:
      {
        move: "1"
        new_state:
          {
            data: "43 2"
            signatures: "sgn1"
          }
      }
  )"));
}

TEST_F (StateProofTests, MissingSignature)
{
  EXPECT_FALSE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
      }
    transitions:
      {
        move: "2"
        new_state:
          {
            data: "44 6"
            signatures: "sgn0"
          }
      }
    transitions:
      {
        move: "2"
        new_state:
          {
            data: "46 7"
            signatures: "sgn0"
          }
      }
  )"));
}

TEST_F (StateProofTests, IntermediateStateOnChain)
{
  ASSERT_TRUE (VerifyProof (" 44 11 ", R"(
    initial_state:
      {
        data: "42 10"
      }
    transitions:
      {
        move: "2"
        new_state:
          {
            data: "44 11"
            signatures: "sgn0"
          }
      }
    transitions:
      {
        move: "2"
        new_state:
          {
            data: "46 12"
            signatures: "sgn0"
          }
      }
  )"));
  EXPECT_EQ (endState, "46 12");
}

TEST_F (StateProofTests, SignedInitialState)
{
  ASSERT_TRUE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn1"
      }
    transitions:
      {
        move: "1"
        new_state:
          {
            data: "43 6"
            signatures: "sgn0"
          }
      }
  )"));
  EXPECT_EQ (endState, "43 6");
}

TEST_F (StateProofTests, MultiSignedLaterState)
{
  ASSERT_TRUE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
      }
    transitions:
      {
        move: "1"
        new_state:
          {
            data: "43 6"
            signatures: "sgn0"
            signatures: "sgn1"
          }
      }
  )"));
  EXPECT_EQ (endState, "43 6");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
