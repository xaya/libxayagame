// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelgame.hpp"

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

  proto::ChannelMetadata meta;

  ChannelsTable tbl;

protected:

  ChannelGameTests ()
    : tbl(game)
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
