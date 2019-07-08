// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stateproof.hpp"

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>

#include <google/protobuf/text_format.h>

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;
using testing::Throw;

proto::StateProof
TextProof (const std::string& str)
{
  proto::StateProof res;
  CHECK (TextFormat::ParseFromString (str, &res));
  return res;
}

class GeneralStateProofTests : public TestGameFixture
{

protected:

  proto::ChannelMetadata meta;
  const uint256 channelId = SHA256::Hash ("channel id");

  GeneralStateProofTests ()
  {
    meta.set_reinit ("reinit");
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

    return VerifyStateTransition (rpcClient, game.rules, channelId, meta,
                                  oldState, proto);
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
  ExpectSignature (channelId, meta, "state",
                   " 11 2 ", "signed by zero", "addr0");

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
    return VerifyStateProof (rpcClient, game.rules, channelId, meta, chainState,
                             TextProof (proof), endState);
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
  ExpectSignature (channelId, meta, "state", "42 5", "signature 0", "addr0");
  ExpectSignature (channelId, meta, "state", "42 5", "signature 1", "addr1");

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

using UnverifiedProofEndStateTests = testing::Test;

TEST_F (UnverifiedProofEndStateTests, InitialState)
{
  EXPECT_EQ (UnverifiedProofEndState (TextProof (R"(
    initial_state: { data: "42 5" }
  )")), "42 5");
}

TEST_F (UnverifiedProofEndStateTests, LastTransition)
{
  EXPECT_EQ (UnverifiedProofEndState (TextProof (R"(
    initial_state: { data: "42 5" }
    transitions:
      {
        new_state: { data: "50 6" }
      }
    transitions:
      {
        new_state: { data: "90 10" }
      }
  )")), "90 10");
}

/* ************************************************************************** */

class ExtendStateProofTests : public GeneralStateProofTests
{

protected:

  proto::StateProof newProof;

  bool
  ExtendProof (const std::string& oldProof, const BoardMove& mv)
  {
    return ExtendStateProof (rpcClient, rpcWallet, game.rules, channelId, meta,
                             TextProof (oldProof), mv, newProof);
  }

};

TEST_F (ExtendStateProofTests, NoTurnState)
{
  EXPECT_FALSE (ExtendProof (R"(
    initial_state:
      {
        data: "100 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )", "1"));
}

TEST_F (ExtendStateProofTests, InvalidMove)
{
  EXPECT_FALSE (ExtendProof (R"(
    initial_state:
      {
        data: "10 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )", "invalid move"));
}

TEST_F (ExtendStateProofTests, SignatureFailure)
{
  EXPECT_CALL (mockXayaWallet, signmessage ("addr0", _))
      .WillOnce (Throw (jsonrpc::JsonRpcException (-5)));

  EXPECT_FALSE (ExtendProof (R"(
    initial_state:
      {
        data: "10 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )", "5"));
}

TEST_F (ExtendStateProofTests, Valid)
{
  EXPECT_CALL (mockXayaWallet, signmessage ("addr0", _))
      .WillRepeatedly (Return (EncodeBase64 ("sgn0")));
  EXPECT_CALL (mockXayaWallet, signmessage ("addr1", _))
      .WillRepeatedly (Return (EncodeBase64 ("sgn1")));

  struct Test
  {
    std::string name;
    std::string oldProof;
    std::string mv;
    std::string newState;
    unsigned numTrans;
  };
  const Test tests[] =
    {

      {
        "from reinit",
        R"(
          initial_state: { data: "0 0" }
        )",
        "42",
        "42 1",
        1,
      },

      {
        "still keep initial",
        R"(
          initial_state:
            {
              data: "10 2"
              signatures: "sgn1"
            }
          transitions:
            {
              move: "4"
              new_state:
                {
                  data: "14 3"
                  signatures: "sgn0"
                }
            }
        )",
        "5",
        "19 4",
        2,
      },

      {
        "removing previous step",
        R"(
          initial_state:
            {
              data: "10 2"
              signatures: "sgn 1"
            }
          transitions:
            {
              move: "1"
              new_state:
                {
                  data: "11 3"
                  signatures: "sgn0"
                }
            }
        )",
        "1",
        "12 4",
        1,
      }

    };

  for (const auto& t : tests)
    {
      LOG (INFO) << "Test case: " << t.name;
      ASSERT_TRUE (ExtendProof (t.oldProof, t.mv));
      EXPECT_EQ (newProof.transitions_size (), t.numTrans);

      BoardState provenState;
      CHECK (VerifyStateProof (rpcClient, game.rules, channelId, meta, "0 0",
                               newProof, provenState));
      auto p = game.rules.ParseState (channelId, meta, provenState);
      CHECK (p != nullptr);
      EXPECT_TRUE (p->Equals (t.newState));
    }
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
