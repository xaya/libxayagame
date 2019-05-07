// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "database.hpp"

#include "testgame.hpp"

#include <xayautil/hash.hpp>

#include <gtest/gtest.h>

namespace xaya
{
namespace
{

class ChannelDbTests : public TestGameFixture
{

protected:

  ChannelsTable tbl;

  ChannelDbTests ()
    : tbl(game)
  {}

};

TEST_F (ChannelDbTests, Creating)
{
  auto h = tbl.CreateNew (SHA256::Hash ("id"));
  h->MutableMetadata ().add_participants ()->set_name ("domob");
  h->SetState ("state");
  h->SetDisputeHeight (1234);
  h.reset ();

  tbl.CreateNew (SHA256::Hash ("default"));

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "domob");
  EXPECT_EQ (h->GetState (), "state");
  ASSERT_TRUE (h->HasDispute ());
  EXPECT_EQ (h->GetDisputeHeight (), 1234);

  h = tbl.GetById (SHA256::Hash ("default"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("default"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 0);
  EXPECT_EQ (h->GetState (), "");
  EXPECT_FALSE (h->HasDispute ());
}

TEST_F (ChannelDbTests, Updating)
{
  auto h = tbl.CreateNew (SHA256::Hash ("id"));
  h->MutableMetadata ().add_participants ();
  h->SetState ("state");
  h->SetDisputeHeight (1234);
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetState (), "state");
  ASSERT_TRUE (h->HasDispute ());
  EXPECT_EQ (h->GetDisputeHeight (), 1234);

  h->MutableMetadata ().Clear ();
  h->SetState ("other state");
  h->ClearDispute ();
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 0);
  EXPECT_EQ (h->GetState (), "other state");
  EXPECT_FALSE (h->HasDispute ());
}

TEST_F (ChannelDbTests, GetByUnknownId)
{
  EXPECT_EQ (tbl.GetById (SHA256::Hash ("foo")), nullptr);
}

TEST_F (ChannelDbTests, DeleteById)
{
  tbl.CreateNew (SHA256::Hash ("first"))->SetState ("first state");
  tbl.CreateNew (SHA256::Hash ("second"))->SetState ("second state");

  tbl.DeleteById (SHA256::Hash ("invalid"));
  tbl.DeleteById (SHA256::Hash ("first"));

  EXPECT_EQ (tbl.GetById (SHA256::Hash ("first")), nullptr);
  auto h = tbl.GetById (SHA256::Hash ("second"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetState (), "second state");

  tbl.DeleteById (SHA256::Hash ("second"));
  EXPECT_EQ (tbl.GetById (SHA256::Hash ("first")), nullptr);
  EXPECT_EQ (tbl.GetById (SHA256::Hash ("second")), nullptr);
}

TEST_F (ChannelDbTests, QueryAll)
{
  const uint256 id1 = SHA256::Hash ("first");
  const uint256 id2 = SHA256::Hash ("second");
  ASSERT_LT (id2.ToHex (), id1.ToHex ());

  tbl.CreateNew (id1);
  tbl.CreateNew (id2);

  auto* stmt = tbl.QueryAll ();
  ASSERT_EQ (sqlite3_step (stmt), SQLITE_ROW);
  EXPECT_EQ (tbl.GetFromResult (stmt)->GetId (), id2);
  ASSERT_EQ (sqlite3_step (stmt), SQLITE_ROW);
  EXPECT_EQ (tbl.GetFromResult (stmt)->GetId (), id1);
  EXPECT_EQ (sqlite3_step (stmt), SQLITE_DONE);
}

TEST_F (ChannelDbTests, QueryForDisputeHeight)
{
  const uint256 id1 = SHA256::Hash ("first");
  const uint256 id2 = SHA256::Hash ("second");
  const uint256 id3 = SHA256::Hash ("third");
  const uint256 id4 = SHA256::Hash ("fourth");
  ASSERT_LT (id2.ToHex (), id1.ToHex ());

  tbl.CreateNew (id1)->SetDisputeHeight (10);
  tbl.CreateNew (id2)->SetDisputeHeight (15);
  tbl.CreateNew (id3)->SetDisputeHeight (16);
  tbl.CreateNew (id4)->ClearDispute ();

  auto* stmt = tbl.QueryForDisputeHeight (15);
  ASSERT_EQ (sqlite3_step (stmt), SQLITE_ROW);
  EXPECT_EQ (tbl.GetFromResult (stmt)->GetId (), id2);
  ASSERT_EQ (sqlite3_step (stmt), SQLITE_ROW);
  EXPECT_EQ (tbl.GetFromResult (stmt)->GetId (), id1);
  EXPECT_EQ (sqlite3_step (stmt), SQLITE_DONE);
}

} // anonymous namespace
} // namespace xaya
