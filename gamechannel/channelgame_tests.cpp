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
   * Creates or retrieves a handle for a channel based on a given "name".
   * The channel's ID is derived from the name, so that multiple channels
   * can be used in tests as needed.  The channel has its metadata set to
   * the "meta" default instance, else no changes are made.
   */
  ChannelsTable::Handle
  GetChannel (const std::string& name)
  {
    const uint256 id = SHA256::Hash (name);

    auto h = tbl.GetById (id);
    if (h == nullptr)
      h = tbl.CreateNew (id);

    h->MutableMetadata () = meta;

    return h;
  }

};

/* ************************************************************************** */

using DisputeTests = ChannelGameTests;

TEST_F (DisputeTests, InvalidStateProof)
{
  auto ch = GetChannel ("test");
  ch->SetState ("0 1");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "0 1");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, InvalidStateClaimed)
{
  auto ch = GetChannel ("test");
  ch->SetState ("0 1");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "invalid"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "0 1");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, NoLaterTurnPreviousDispute)
{
  auto ch = GetChannel ("test");
  ch->SetDisputeHeight (50);
  ch->SetState ("10 5");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "10 5");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 50);
}

TEST_F (DisputeTests, EarlierTurnPreviousResolution)
{
  auto ch = GetChannel ("test");
  ch->SetState ("10 5");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 4"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "10 5");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, NoTurnState)
{
  auto ch = GetChannel ("test");
  ch->SetState ("100 5");

  ASSERT_FALSE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "101 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "100 5");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (DisputeTests, SettingValidDispute)
{
  auto ch = GetChannel ("test");
  ch->SetState ("10 5");

  ASSERT_TRUE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "20 6");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (DisputeTests, SameTurnAsPreviousResolution)
{
  auto ch = GetChannel ("test");
  ch->SetState ("10 5");

  ASSERT_TRUE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "20 5");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (DisputeTests, UpdateAtSameHeight)
{
  auto ch = GetChannel ("test");
  ch->SetDisputeHeight (100);
  ch->SetState ("10 5");

  ASSERT_TRUE (game.ProcessDispute (*ch, 100, ParseStateProof (R"(
    initial_state:
      {
        data: "20 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "20 6");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

/* ************************************************************************** */

using ResolutionTests = ChannelGameTests;

TEST_F (ResolutionTests, InvalidStateProof)
{
  auto ch = GetChannel ("test");
  ch->SetDisputeHeight (100);
  ch->SetState ("0 1");

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "42 5"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "0 1");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (ResolutionTests, InvalidStateClaimed)
{
  auto ch = GetChannel ("test");
  ch->SetDisputeHeight (100);
  ch->SetState ("0 1");

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "invalid"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "0 1");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (ResolutionTests, NoLaterTurnPreviousDispute)
{
  auto ch = GetChannel ("test");
  ch->SetDisputeHeight (100);
  ch->SetState ("10 5");

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "20 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "10 5");
  ASSERT_TRUE (ch->HasDispute ());
  EXPECT_EQ (ch->GetDisputeHeight (), 100);
}

TEST_F (ResolutionTests, NoLaterTurnPreviousResolution)
{
  auto ch = GetChannel ("test");
  ch->SetState ("10 5");

  ASSERT_FALSE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "20 5"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "10 5");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (ResolutionTests, Valid)
{
  auto ch = GetChannel ("test");
  ch->SetDisputeHeight (100);
  ch->SetState ("10 5");

  ASSERT_TRUE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "20 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "20 6");
  EXPECT_FALSE (ch->HasDispute ());
}

TEST_F (ResolutionTests, ResolvesToNoTurnState)
{
  auto ch = GetChannel ("test");
  ch->SetDisputeHeight (100);
  ch->SetState ("10 5");

  ASSERT_TRUE (game.ProcessResolution (*ch, ParseStateProof (R"(
    initial_state:
      {
        data: "100 6"
        signatures: "sgn0"
        signatures: "sgn1"
      }
  )")));

  EXPECT_EQ (ch->GetState (), "100 6");
  EXPECT_FALSE (ch->HasDispute ());
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
