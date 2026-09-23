// Copyright (C) 2019-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stateproof.hpp"

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <google/protobuf/text_format.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

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
  const std::string gameId = "game id";
  const uint256 channelId = SHA256::Hash ("channel id");

  GeneralStateProofTests ()
  {
    meta.set_reinit ("reinit");
    meta.add_participants ()->set_address ("addr0");
    meta.add_participants ()->set_address ("addr1");

    verifier.SetValid ("sgn0", "addr0");
    verifier.SetValid ("sgn1", "addr1");
    verifier.SetValid ("sgn42", "addr42");
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

    return VerifyStateTransition (verifier, game.rules, gameId, channelId,
                                  meta, oldState, proto);
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
  verifier.ExpectOne (gameId, channelId, meta, "state",
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
    return VerifyStateProof (verifier, game.rules, gameId, channelId, meta,
                             chainState, TextProof (proof), endState);
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
  verifier.ExpectOne (gameId, channelId, meta, "state",
                      "42 5", "signature 0", "addr0");
  verifier.ExpectOne (gameId, channelId, meta, "state",
                      "42 5", "signature 1", "addr1");

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

TEST_F (StateProofTests, DefaultRequiredSignaturesIsAllParticipants)
{
  /* Without an overridden RequiredSignatures, an unanchored proof needs
     signatures of all participants (the historic rule).  */
  EXPECT_FALSE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
      }
  )"));

  ASSERT_TRUE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));
  EXPECT_EQ (endState, "42 5");
}

/* ************************************************************************** */

/**
 * A ParsedBoardState that wraps a state of the addition game, but reports
 * a custom set of required signatures.
 */
class SignerSetState : public ParsedBoardState
{

private:

  /** The underlying parsed addition-game state.  */
  std::unique_ptr<ParsedBoardState> inner;

  /** The set of required signatures to report.  */
  const std::set<int> required;

public:

  explicit SignerSetState (const BoardRules& r, const uint256& id,
                           const proto::ChannelMetadata& m,
                           std::unique_ptr<ParsedBoardState> i,
                           const std::set<int>& req)
    : ParsedBoardState(r, id, m), inner(std::move (i)), required(req)
  {}

  bool
  Equals (const BoardState& other) const override
  {
    return inner->Equals (other);
  }

  int
  WhoseTurn () const override
  {
    return inner->WhoseTurn ();
  }

  unsigned
  TurnCount () const override
  {
    return inner->TurnCount ();
  }

  bool
  ApplyMove (const BoardMove& mv, BoardState& newState) const override
  {
    return inner->ApplyMove (mv, newState);
  }

  std::set<int>
  RequiredSignatures () const override
  {
    return required;
  }

};

/**
 * Board rules that behave like the addition game, except that all parsed
 * states report a configurable set of required signatures.
 */
class SignerSetRules : public BoardRules
{

private:

  /** The underlying addition-game rules.  */
  AdditionRules base;

public:

  /** The set of required signatures reported by all parsed states.  */
  std::set<int> required;

  std::unique_ptr<ParsedBoardState>
  ParseState (const uint256& channelId, const proto::ChannelMetadata& meta,
              const BoardState& s) const override
  {
    auto inner = base.ParseState (channelId, meta, s);
    if (inner == nullptr)
      return nullptr;
    return std::make_unique<SignerSetState> (*this, channelId, meta,
                                             std::move (inner), required);
  }

  ChannelProtoVersion
  GetProtoVersion (const proto::ChannelMetadata& meta) const override
  {
    return base.GetProtoVersion (meta);
  }

};

class SignerSetStateProofTests : public GeneralStateProofTests
{

protected:

  SignerSetRules rules;

  BoardState endState;

  SignerSetStateProofTests ()
  {
    meta.add_participants ()->set_address ("addr2");
    verifier.SetValid ("sgn2", "addr2");
  }

  /**
   * Calls VerifyStateProof with the custom rules based on the given
   * on-chain state and the proof proto loaded from text format.
   */
  bool
  VerifyProof (const BoardState& chainState, const std::string& proof)
  {
    return VerifyStateProof (verifier, rules, gameId, channelId, meta,
                             chainState, TextProof (proof), endState);
  }

};

TEST_F (SignerSetStateProofTests, ExcludedSeatNotNeeded)
{
  rules.required = {0, 2};
  ASSERT_TRUE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn2"
      }
  )"));
  EXPECT_EQ (endState, "42 5");
}

TEST_F (SignerSetStateProofTests, SignaturesAccumulateAcrossTransitions)
{
  rules.required = {0, 2};
  ASSERT_TRUE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn2"
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

TEST_F (SignerSetStateProofTests, MissingRequiredSeat)
{
  rules.required = {0, 2};
  EXPECT_FALSE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));
}

TEST_F (SignerSetStateProofTests, OnChainAcceptedRegardless)
{
  rules.required = {0, 1, 2};
  ASSERT_TRUE (VerifyProof ("42 5", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn42"
      }
  )"));
  EXPECT_EQ (endState, "42 5");
}

TEST_F (SignerSetStateProofTests, UnparseableReinitRequiresAll)
{
  rules.required = {0};

  EXPECT_FALSE (VerifyProof ("invalid", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
      }
  )"));

  ASSERT_TRUE (VerifyProof ("invalid", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
        signatures: "sgn2"
      }
  )"));
  EXPECT_EQ (endState, "42 5");
}

/* ************************************************************************** */

/**
 * Board rules like SignerSetRules, except that the required signatures
 * depend on the content of the parsed state itself:  the state matching
 * a configured content requires all participants, while every other
 * state requires only seat 0.  This makes the reinit state and the states
 * inside a proof report different sets, so that tests can pin down which
 * of them the verification derives the required set from.
 */
class ContentSignerRules : public BoardRules
{

private:

  /** The underlying addition-game rules.  */
  AdditionRules base;

public:

  /** The state content whose parsed state requires all participants.  */
  BoardState allSignersContent;

  std::unique_ptr<ParsedBoardState>
  ParseState (const uint256& channelId, const proto::ChannelMetadata& meta,
              const BoardState& s) const override
  {
    auto inner = base.ParseState (channelId, meta, s);
    if (inner == nullptr)
      return nullptr;

    std::set<int> required;
    if (inner->Equals (allSignersContent))
      for (int i = 0; i < meta.participants_size (); ++i)
        required.insert (i);
    else
      required.insert (0);

    return std::make_unique<SignerSetState> (*this, channelId, meta,
                                             std::move (inner), required);
  }

  ChannelProtoVersion
  GetProtoVersion (const proto::ChannelMetadata& meta) const override
  {
    return base.GetProtoVersion (meta);
  }

};

class ContentSignerStateProofTests : public GeneralStateProofTests
{

protected:

  ContentSignerRules rules;

  BoardState endState;

  ContentSignerStateProofTests ()
  {
    meta.add_participants ()->set_address ("addr2");
    verifier.SetValid ("sgn2", "addr2");
  }

  /**
   * Calls VerifyStateProof with the custom rules based on the given
   * on-chain state and the proof proto loaded from text format.
   */
  bool
  VerifyProof (const BoardState& chainState, const std::string& proof)
  {
    return VerifyStateProof (verifier, rules, gameId, channelId, meta,
                             chainState, TextProof (proof), endState);
  }

};

TEST_F (ContentSignerStateProofTests, RequiredSetDerivedFromReinitState)
{
  rules.allSignersContent = "0 1";

  /* The reinit state ("0 1") requires all three participants, while the
     proof's own states require just seat 0.  The required set must be
     derived from the reinit state and never from the proof's content, so
     a proof signed only by seat 0 is missing seats 1 and 2 and gets
     rejected — even though the set derived from the proof's end state
     would be satisfied.  */
  EXPECT_FALSE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
      }
  )"));

  /* With all three signatures present, the same proof is fine.  This
     ensures the rejection above is really due to the reinit-derived set
     and not some other defect.  */
  ASSERT_TRUE (VerifyProof ("0 1", R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
        signatures: "sgn2"
      }
  )"));
  EXPECT_EQ (endState, "42 5");
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
    return ExtendStateProof (verifier, signer, game.rules, gameId, channelId,
                             meta, TextProof (oldProof), mv, newProof);
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
  signer.SetAddress ("invalid address");

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
  signer.SetAddress ("addr0");
  EXPECT_CALL (signer, SignMessage (_)).WillRepeatedly (Return ("sgn0"));

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
              data: "11 2"
              signatures: "sgn0"
            }
          transitions:
            {
              move: "1"
              new_state:
                {
                  data: "12 3"
                  signatures: "sgn1"
                }
            }
        )",
        "1",
        "13 4",
        1,
      },

    };

  for (const auto& t : tests)
    {
      LOG (INFO) << "Test case: " << t.name;
      ASSERT_TRUE (ExtendProof (t.oldProof, t.mv));
      EXPECT_EQ (newProof.transitions_size (), t.numTrans);

      BoardState provenState;
      CHECK (VerifyStateProof (verifier, game.rules, gameId, channelId, meta,
                               "0 0", newProof, provenState));
      auto p = game.rules.ParseState (channelId, meta, provenState);
      CHECK (p != nullptr);
      EXPECT_TRUE (p->Equals (t.newState));
    }
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
