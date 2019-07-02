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

  proto::ChannelMetadata meta;

  ChannelDbTests ()
    : tbl(game)
  {}

};

TEST_F (ChannelDbTests, Creating)
{
  meta.add_participants ()->set_name ("domob");

  auto h = tbl.CreateNew (SHA256::Hash ("id"));
  h->Reinitialise (meta, "state");
  h->SetDisputeHeight (1234);
  h.reset ();

  h = tbl.CreateNew (SHA256::Hash ("default"));
  h->Reinitialise (proto::ChannelMetadata (), "");
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "domob");
  EXPECT_EQ (h->GetReinitState (), "state");
  EXPECT_EQ (h->GetLatestState (), "state");
  ASSERT_TRUE (h->HasDispute ());
  EXPECT_EQ (h->GetDisputeHeight (), 1234);

  h = tbl.GetById (SHA256::Hash ("default"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("default"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 0);
  EXPECT_EQ (h->GetReinitState (), "");
  EXPECT_EQ (h->GetLatestState (), "");
  EXPECT_FALSE (h->HasDispute ());
}

TEST_F (ChannelDbTests, UpdatingWithReinit)
{
  meta.add_participants ();

  auto h = tbl.CreateNew (SHA256::Hash ("id"));
  h->Reinitialise (meta, "state");
  h->SetDisputeHeight (1234);
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().reinit (), "");
  EXPECT_EQ (h->GetReinitState (), "state");
  EXPECT_EQ (h->GetLatestState (), "state");
  ASSERT_TRUE (h->HasDispute ());
  EXPECT_EQ (h->GetDisputeHeight (), 1234);

  meta.Clear ();
  meta.set_reinit ("init 2");
  h->Reinitialise (meta, "other state");
  h->ClearDispute ();
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 0);
  EXPECT_EQ (h->GetMetadata ().reinit (), "init 2");
  EXPECT_EQ (h->GetReinitState (), "other state");
  EXPECT_EQ (h->GetLatestState (), "other state");
  EXPECT_FALSE (h->HasDispute ());
}

TEST_F (ChannelDbTests, UpdatingWithStateProof)
{
  auto h = tbl.CreateNew (SHA256::Hash ("id"));
  h->Reinitialise (meta, "state");
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetReinitState (), "state");
  EXPECT_EQ (h->GetLatestState (), "state");

  proto::StateProof proof;
  proof.add_transitions ()->mutable_new_state ()->set_data ("other state");
  h->SetStateProof (proof);
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetReinitState (), "state");
  EXPECT_EQ (h->GetLatestState (), "other state");
}

TEST_F (ChannelDbTests, StringsWithNul)
{
  const std::string str1("a\0b", 3);
  const std::string str2("x\0y", 3);
  ASSERT_EQ (str1.size (), 3);
  ASSERT_EQ (str1[1], 0);
  ASSERT_EQ (str2.size (), 3);
  ASSERT_EQ (str2[1], 0);

  meta.add_participants ()->set_name (str1);
  auto h = tbl.CreateNew (SHA256::Hash ("id"));
  h->Reinitialise (meta, str2);
  h.reset ();

  h = tbl.GetById (SHA256::Hash ("id"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetId (), SHA256::Hash ("id"));
  EXPECT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), str1);
  EXPECT_EQ (h->GetReinitState (), str2);
  EXPECT_EQ (h->GetLatestState (), str2);
}

TEST_F (ChannelDbTests, GetByUnknownId)
{
  EXPECT_EQ (tbl.GetById (SHA256::Hash ("foo")), nullptr);
}

TEST_F (ChannelDbTests, DeleteById)
{
  tbl.CreateNew (SHA256::Hash ("first"))->Reinitialise (meta, "first state");
  tbl.CreateNew (SHA256::Hash ("second"))->Reinitialise (meta, "second state");

  tbl.DeleteById (SHA256::Hash ("invalid"));
  tbl.DeleteById (SHA256::Hash ("first"));

  EXPECT_EQ (tbl.GetById (SHA256::Hash ("first")), nullptr);
  auto h = tbl.GetById (SHA256::Hash ("second"));
  ASSERT_NE (h, nullptr);
  EXPECT_EQ (h->GetLatestState (), "second state");

  tbl.DeleteById (SHA256::Hash ("second"));
  EXPECT_EQ (tbl.GetById (SHA256::Hash ("first")), nullptr);
  EXPECT_EQ (tbl.GetById (SHA256::Hash ("second")), nullptr);
}

TEST_F (ChannelDbTests, QueryAll)
{
  const uint256 id1 = SHA256::Hash ("first");
  const uint256 id2 = SHA256::Hash ("second");
  ASSERT_LT (id2.ToHex (), id1.ToHex ());

  tbl.CreateNew (id1)->Reinitialise (meta, "foo");
  tbl.CreateNew (id2)->Reinitialise (meta, "bar");

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

  auto h = tbl.CreateNew (id1);
  h->Reinitialise (meta, "");
  h->SetDisputeHeight (10);
  h.reset ();

  h = tbl.CreateNew (id2);
  h->Reinitialise (meta, "");
  h->SetDisputeHeight (15);
  h.reset ();

  h = tbl.CreateNew (id3);
  h->Reinitialise (meta, "");
  h->SetDisputeHeight (16);
  h.reset ();

  h = tbl.CreateNew (id4);
  h->Reinitialise (meta, "");
  h->ClearDispute ();
  h.reset ();

  auto* stmt = tbl.QueryForDisputeHeight (15);
  ASSERT_EQ (sqlite3_step (stmt), SQLITE_ROW);
  EXPECT_EQ (tbl.GetFromResult (stmt)->GetId (), id2);
  ASSERT_EQ (sqlite3_step (stmt), SQLITE_ROW);
  EXPECT_EQ (tbl.GetFromResult (stmt)->GetId (), id1);
  EXPECT_EQ (sqlite3_step (stmt), SQLITE_DONE);
}

} // anonymous namespace
} // namespace xaya
