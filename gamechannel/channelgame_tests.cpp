// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelgame.hpp"

#include "protoutils.hpp"
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
 * Parses a text-format string into a StateProof proto.
 */
proto::StateProof
ParseStateProof (const std::string& str)
{
  proto::StateProof res;
  CHECK (TextFormat::ParseFromString (str, &res));

  return res;
}

class ChannelGameTests : public TestGameFixture
{

private:

  ChannelsTable tbl;

protected:

  proto::ChannelMetadata meta;

  ChannelGameTests ()
    : tbl(GetDb ())
  {
    meta.add_participants ()->set_address ("addr0");
    meta.add_participants ()->set_address ("addr1");

    ValidSignature ("sgn0", "addr0");
    ValidSignature ("sgn1", "addr1");
    ValidSignature ("sgn42", "addr42");
  }

  /**
   * Creates a new test channel, initialised to the given state and
   * with our metadata.
   */
  ChannelsTable::Handle
  CreateChannel (const std::string& name, const BoardState& initial)
  {
    const uint256 id = SHA256::Hash (name);
    auto h = tbl.CreateNew (id);
    h->Reinitialise (meta, initial);
    return h;
  }

};

/* ************************************************************************** */

using DisputeTests = ChannelGameTests;

TEST_F (DisputeTests, InvalidStateProof)
{
  auto ch = CreateChannel ("test", "0 1");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "0 1");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, InvalidProtoVersion)
{
  auto ch = CreateChannel ("test", "0 1");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
        for_testing_version: "foo"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "0 1");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, InvalidStateClaimed)
{
  auto ch = CreateChannel ("test", "0 1");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "invalid"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "0 1");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, EarlierTurn)
{
  auto ch = CreateChannel ("test", "2 2");

  /* The reinit state would be early enough (earlier than the resolution),
     but the latest state we set is not.  This verifies that we use the latest
     state for the turn-count check.  */
  ch->SetStateProof (ParseStateProof (R"(
    initial_state: { data: "10 5" }
  )"));
  ASSERT_EQ (ch->GetReinitState (), "2 2");
  ASSERT_EQ (ch->GetLatestState (), "10 5");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 4"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "10 5");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, SameTurnAndDifferentState)
{
  auto ch = CreateChannel ("test", "10 5");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "10 5");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, SameTurnAndStatePreviousDispute)
{
  auto ch = CreateChannel ("test", "10 5");
  ch->SetDisputeHeight (50);

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "10 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "10 5");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 50);
}

TEST_F (DisputeTests, NoTurnState)
{
  auto ch = CreateChannel ("test", "100 5");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "101 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "100 5");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, SettingValidDispute)
{
  auto ch = CreateChannel ("test", "10 5");

  /* Set a different latest state than the reinit state.  The state proof
     based on the reinit state should be valid.  */
  ch->SetStateProof (ParseStateProof (R"(
    initial_state: { data: "15 5" }
  )"));
  ASSERT_EQ (ch->GetReinitState (), "10 5");
  ASSERT_EQ (ch->GetLatestState (), "15 5");

  ASSERT_TRUE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state: { data: "10 5" }
    transitions:
      {
        move: "10"
        new_state:
          {
            data: "20 6"
            signatures: "sgn0"
          }
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "20 6");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (DisputeTests, SameTurnAndStatePreviousResolution)
{
  auto ch = CreateChannel ("test", "10 5");

  ASSERT_TRUE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "  10 5  "
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "10 5");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (DisputeTests, UpdateAtSameHeight)
{
  auto ch = CreateChannel ("test", "10 5");
  ch->SetDisputeHeight (100);

  ASSERT_TRUE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "20 6");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

/* ************************************************************************** */

using ResolutionTests = ChannelGameTests;

TEST_F (ResolutionTests, InvalidStateProof)
{
  auto ch = CreateChannel ("test", "0 1");
  ch->SetDisputeHeight (100);

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "0 1");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (ResolutionTests, InvalidProtoVersion)
{
  auto ch = CreateChannel ("test", "0 1");
  ch->SetDisputeHeight (100);

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
        for_testing_version: "foo"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "0 1");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (ResolutionTests, InvalidStateClaimed)
{
  auto ch = CreateChannel ("test", "0 1");
  ch->SetDisputeHeight (100);

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "invalid"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "0 1");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (ResolutionTests, NoLaterTurn)
{
  auto ch = CreateChannel ("test", "4 4");
  ch->SetDisputeHeight (100);

  /* The reinit state would be early enough (earlier than the resolution),
     but the latest state we set is not.  This verifies that we use the latest
     state for the turn-count check.  */
  ch->SetStateProof (ParseStateProof (R"(
    initial_state: { data: "10 5" }
  )"));
  ASSERT_EQ (ch->GetReinitState (), "4 4");
  ASSERT_EQ (ch->GetLatestState (), "10 5");

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "20 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "10 5");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (ResolutionTests, Valid)
{
  auto ch = CreateChannel ("test", "10 5");
  ch->SetDisputeHeight (100);

  /* Set a different latest state than the reinit state.  The state proof
     based on the reinit state should be valid.  */
  ch->SetStateProof (ParseStateProof (R"(
    initial_state: { data: "15 5" }
  )"));
  ASSERT_EQ (ch->GetReinitState (), "10 5");
  ASSERT_EQ (ch->GetLatestState (), "15 5");

  ASSERT_TRUE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state: { data: "10 5" }
    transitions:
      {
        move: "10"
        new_state:
          {
            data: "20 6"
            signatures: "sgn0"
          }
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "20 6");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (ResolutionTests, ResolvesToNoTurnState)
{
  auto ch = CreateChannel ("test", "10 5");
  ch->SetDisputeHeight (100);

  ASSERT_TRUE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "100 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetLatestState (), "100 6");
  EXPECT_FALSE (ch->HasDispute ());
}

/* ************************************************************************** */

/**
 * Fake implementation of PendingMoves for our test game.
 */
class TestPendingMoves : public ChannelGame::PendingMoves
{

protected:

  /**
   * We need to implement AddPendingMove so that we can instantiate this
   * class.  This method will never be called, though, as we just call the
   * methods from ChannelGame::PendingMoves directly to test them.  (We do not
   * even have a move format defined for the test game at all.)
   */
  void
  AddPendingMove (const Json::Value& mv) override
  {
    LOG (FATAL) << "AddPendingMove is not implemented";
  }

public:

  explicit TestPendingMoves (TestGame& g)
    : PendingMoves(g)
  {}

  using ChannelGame::PendingMoves::Clear;
  using ChannelGame::PendingMoves::AddPendingStateProof;

};

class PendingMovesTests : public ChannelGameTests
{

protected:

  TestPendingMoves proc;

  PendingMovesTests ()
    : proc(game)
  {
    proc.InitialiseGameContext (Chain::MAIN, "add",
                                &mockXayaServer.GetClient ());
  }

  /**
   * Expects that the pending state has exactly the given states for
   * all the channels.  IDs are passed as strings that are hashed (similar
   * to CreateChannel).
   *
   * Since we cannot provide the exact "expected" representation of the
   * base64-encoded StateProof, we instead modify the JSON that is compared
   * in the end so as to only compare the represented state.  That is good
   * enough for our purposes.
   */
  void
  ExpectPendingChannels (const std::map<std::string, std::string>& expected)
  {
    Json::Value expectedJson(Json::objectValue);
    for (const auto& entry : expected)
      {
        const uint256 id = SHA256::Hash (entry.first);

        Json::Value val(Json::objectValue);
        val["id"] = id.ToHex ();
        val["state"] = entry.second;

        auto p = game.rules.ParseState (id, meta, entry.second);
        CHECK (p != nullptr);
        val["turncount"] = p->TurnCount ();

        expectedJson[id.ToHex ()] = val;
      }

    auto actual = proc.ToJson ()["channels"];
    ASSERT_TRUE (actual.isObject ());
    for (auto& entry : actual)
      {
        proto::StateProof proof;
        ASSERT_TRUE (ProtoFromBase64 (entry["proof"].asString (), proof));
        entry.removeMember ("proof");
        entry["state"] = UnverifiedProofEndState (proof);
      }

    EXPECT_EQ (actual, expectedJson);
  }

};

TEST_F (PendingMovesTests, InvalidStateProof)
{
  auto ch = CreateChannel ("test", "0 1");
  proc.AddPendingStateProof (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
      }
  )"));
  ExpectPendingChannels ({});
}

TEST_F (PendingMovesTests, InvalidProtoVersion)
{
  auto ch = CreateChannel ("test", "0 1");
  proc.AddPendingStateProof (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
        for_testing_version: "foo"
      }
  )"));
  ExpectPendingChannels ({});
}

TEST_F (PendingMovesTests, NotLater)
{
  auto ch1 = CreateChannel ("foo", "0 1");
  proc.AddPendingStateProof (*ch1, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));
  auto ch2 = CreateChannel ("bar", "10 2");

  /* The following two pending moves are valid, but do not have later turns
     than the existing pending (ch1) or on-chain (ch2) state.  */
  proc.AddPendingStateProof (*ch1, ParseStateProof (R"(
    initial_state:
      {
        data: "55 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));
  proc.AddPendingStateProof (*ch2, ParseStateProof (R"(
    initial_state:
      {
        data: "42 2"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));

  ExpectPendingChannels ({
    {"foo", "42 5"},
  });
}

TEST_F (PendingMovesTests, AddingAndClear)
{
  auto ch1 = CreateChannel ("foo", "0 1");
  proc.AddPendingStateProof (*ch1, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));
  auto ch2 = CreateChannel ("bar", "10 2");

  /* The following two pending moves are valid and have later turn counts than
     the existing pending (ch1) or on-chain (ch2) state.  */
  proc.AddPendingStateProof (*ch1, ParseStateProof (R"(
    initial_state:
      {
        data: "55 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));
  proc.AddPendingStateProof (*ch2, ParseStateProof (R"(
    initial_state:
      {
        data: "42 3"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )"));

  ExpectPendingChannels ({
    {"foo", "55 6"},
    {"bar", "42 3"},
  });

  proc.Clear ();
  ExpectPendingChannels ({});
}

/* ************************************************************************** */

TEST (UpdateMetadataReinitTests, Works)
{
  proto::ChannelMetadata meta;
  const std::string reinit1 = meta.reinit ();

  UpdateMetadataReinit (SHA256::Hash ("foo"), meta);
  const std::string reinit2 = meta.reinit ();

  UpdateMetadataReinit (SHA256::Hash ("bar"), meta);
  const std::string reinit3 = meta.reinit ();

  EXPECT_NE (reinit1, reinit2);
  EXPECT_NE (reinit1, reinit3);
  EXPECT_NE (reinit2, reinit3);

  uint256 val;
  val.FromBlob (reinterpret_cast<const unsigned char*> (reinit2.data ()));
  EXPECT_EQ (val.ToHex (),
      "c7ade88fc7a21498a6a5e5c385e1f68bed822b72aa63c4a9a48a02c2466ee29e");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
