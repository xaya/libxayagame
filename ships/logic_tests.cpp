// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include "proto/boardstate.pb.h"
#include "testutils.hpp"

#include <gamechannel/database.hpp>
#include <xayautil/hash.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace ships
{

class StateUpdateTests : public InMemoryLogicFixture
{

protected:

  xaya::ChannelsTable tbl;

  StateUpdateTests ()
    : tbl(game)
  {}

  /**
   * This calls UpdateState with the given sequence of moves and claiming
   * the given block height.
   */
  void
  UpdateState (const unsigned height, const std::vector<Json::Value>& moves)
  {
    Json::Value block(Json::objectValue);
    block["height"] = height;

    Json::Value moveJson(Json::arrayValue);
    for (const auto& mv : moves)
      moveJson.append (mv);

    Json::Value blockData(Json::objectValue);
    blockData["block"] = block;
    blockData["moves"] = moveJson;

    game.UpdateState (nullptr, blockData);
  }

  /**
   * Expects that the number of open channels is the given one.  This can be
   * used to verify there are no unexpected channels.
   */
  void
  ExpectNumberOfChannels (const unsigned expected)
  {
    unsigned actual = 0;
    sqlite3_stmt* stmt = tbl.QueryAll ();
    while (true)
      {
        const int rc = sqlite3_step (stmt);
        if (rc == SQLITE_DONE)
          break;

        ASSERT_EQ (rc, SQLITE_ROW);
        ++actual;
      }

    EXPECT_EQ (actual, expected);
  }

  /**
   * Expects that a channel with the given ID exists and returns the handle
   * to it.
   */
  xaya::ChannelsTable::Handle
  ExpectChannel (const xaya::uint256& id)
  {
    auto h = tbl.GetById (id);
    CHECK (h != nullptr);
    return h;
  }

};

namespace
{

/**
 * Utility method to construct a move object from the ingredients (name,
 * txid and actual move data).
 */
Json::Value
Move (const std::string& name, const xaya::uint256& txid,
      const Json::Value& data)
{
  Json::Value res(Json::objectValue);
  res["name"] = name;
  res["txid"] = txid.ToHex ();
  res["move"] = data;

  return res;
}

TEST_F (StateUpdateTests, MoveNotAnObject)
{
  const auto txid = xaya::SHA256::Hash ("foo");

  std::vector<Json::Value> moves;
  for (const std::string& mv : {"10", "\"foo\"", "null", "true", "[42]"})
    moves.push_back (Move ("foo", txid, ParseJson (mv)));

  UpdateState (10, moves);
  ExpectNumberOfChannels (0);
}

TEST_F (StateUpdateTests, MultipleActions)
{
  UpdateState (10, {Move ("foo", xaya::uint256 (), ParseJson (R"(
    {
      "c": {"addr": "my address"},
      "x": "something else"
    }
  )"))});
  ExpectNumberOfChannels (0);
}

TEST_F (StateUpdateTests, InvalidMoveContinuesProcessing)
{
  UpdateState (10, {
    Move ("foo", xaya::SHA256::Hash ("foo"), ParseJson ("\"foo\"")),
    Move ("bar", xaya::SHA256::Hash ("bar"), ParseJson (R"(
      {
        "c": {"addr": "my address"}
      }
    )")),
  });
  ExpectNumberOfChannels (1);
}

/* ************************************************************************** */

using CreateChannelTests = StateUpdateTests;

TEST_F (CreateChannelTests, InvalidCreates)
{
  const auto txid = xaya::SHA256::Hash ("foo");

  std::vector<Json::Value> moves;
  for (const std::string& create : {"42", "null", "{}",
                                    R"({"addr": 100})",
                                    R"({"addr": "foo", "x": 5})"})
    {
      Json::Value data(Json::objectValue);
      data["c"] = ParseJson (create);
      moves.push_back (Move ("foo", txid, data));
    }

  UpdateState (10, moves);
  ExpectNumberOfChannels (0);
}

TEST_F (CreateChannelTests, CreationSuccessful)
{
  UpdateState (10, {
    Move ("foo", xaya::SHA256::Hash ("foo"), ParseJson ("\"invalid\"")),
    Move ("bar", xaya::SHA256::Hash ("bar"), ParseJson (R"(
      {"c": {"addr": "address 1"}}
    )")),
    Move ("bar", xaya::SHA256::Hash ("baz"), ParseJson (R"(
      {"c": {"addr": "address 2"}}
    )")),
    Move ("bar", xaya::SHA256::Hash ("bah"), ParseJson (R"(
      {"c": {"addr": "address 2"}}
    )")),
  });

  ExpectNumberOfChannels (3);

  auto h = ExpectChannel (xaya::SHA256::Hash ("bar"));
  ASSERT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "bar");
  EXPECT_EQ (h->GetMetadata ().participants (0).address (), "address 1");
  EXPECT_EQ (h->GetState (), "");
  EXPECT_FALSE (h->HasDispute ());

  h = ExpectChannel (xaya::SHA256::Hash ("baz"));
  ASSERT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "bar");
  EXPECT_EQ (h->GetMetadata ().participants (0).address (), "address 2");

  h = ExpectChannel (xaya::SHA256::Hash ("bah"));
  ASSERT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "bar");
  EXPECT_EQ (h->GetMetadata ().participants (0).address (), "address 2");
}

TEST_F (CreateChannelTests, FailsForTxidCollision)
{
  /* Turn off the mock server.  We don't need it, and it complicates the
     death test due to extra threads.  */
  mockXayaServer.StopListening ();

  const auto data = ParseJson (R"(
    {"c": {"addr": "address"}}
  )");
  EXPECT_DEATH (UpdateState (10, {
    Move ("foo", xaya::SHA256::Hash ("foo"), data),
    Move ("bar", xaya::SHA256::Hash ("foo"), data),
  }), "Already have channel with ID");
}

/* ************************************************************************** */

using JoinChannelTests = StateUpdateTests;

TEST_F (JoinChannelTests, Malformed)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  tbl.CreateNew (existing)->MutableMetadata ().add_participants ();

  const auto txid = xaya::SHA256::Hash ("bar");

  std::vector<Json::Value> moves;
  for (const std::string& create : {"42", "null", "{}",
                                    R"({"addr": 100, "id": "00"})",
                                    R"({"addr": "addr", "id": 100})",
                                    R"({"addr": "addr", "id": "00"})",
                                    R"({"addr": "foo", "id": "00", "x": 5})"})
    {
      Json::Value data(Json::objectValue);
      data["j"] = ParseJson (create);
      moves.push_back (Move ("foo", txid, data));
    }
  UpdateState (10, moves);

  ExpectNumberOfChannels (1);
  EXPECT_EQ (ExpectChannel (existing)->GetMetadata ().participants_size (), 1);
}

TEST_F (JoinChannelTests, NonExistantChannel)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  tbl.CreateNew (existing)->MutableMetadata ().add_participants ();

  const auto txid = xaya::SHA256::Hash ("bar");
  Json::Value data (Json::objectValue);
  data["j"] = ParseJson (R"({"addr": "address"})");
  data["j"]["id"] = txid.ToHex ();
  UpdateState (10, {Move ("foo", txid, data)});

  ExpectNumberOfChannels (1);
  EXPECT_EQ (ExpectChannel (existing)->GetMetadata ().participants_size (), 1);
}

TEST_F (JoinChannelTests, AlreadyTwoParticipants)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  auto h = tbl.CreateNew (existing);
  h->MutableMetadata ().add_participants ()->set_name ("foo");
  h->MutableMetadata ().add_participants ()->set_name ("bar");
  h.reset ();

  const auto txid = xaya::SHA256::Hash ("bar");
  Json::Value data (Json::objectValue);
  data["j"] = ParseJson (R"({"addr": "address"})");
  data["j"]["id"] = existing.ToHex ();
  UpdateState (10, {Move ("baz", txid, data)});

  ExpectNumberOfChannels (1);
  h = ExpectChannel (existing);
  ASSERT_EQ (h->GetMetadata ().participants_size (), 2);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "foo");
  EXPECT_EQ (h->GetMetadata ().participants (1).name (), "bar");
}

TEST_F (JoinChannelTests, SameNameInChannel)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  auto h = tbl.CreateNew (existing);
  h->MutableMetadata ().add_participants ()->set_name ("foo");
  h.reset ();

  const auto txid = xaya::SHA256::Hash ("bar");
  Json::Value data (Json::objectValue);
  data["j"] = ParseJson (R"({"addr": "address"})");
  data["j"]["id"] = existing.ToHex ();
  UpdateState (10, {Move ("foo", txid, data)});

  ExpectNumberOfChannels (1);
  h = ExpectChannel (existing);
  ASSERT_EQ (h->GetMetadata ().participants_size (), 1);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "foo");
}

TEST_F (JoinChannelTests, SuccessfulJoin)
{
  const auto id1 = xaya::SHA256::Hash ("foo");
  const auto id2 = xaya::SHA256::Hash ("bar");

  std::vector<Json::Value> moves;
  moves.push_back (Move ("foo", id1, ParseJson (R"({"c": {"addr": "a"}})")));

  Json::Value data(Json::objectValue);
  data["j"] = ParseJson (R"({"addr": "b"})");
  data["j"]["id"] = id1.ToHex ();
  moves.push_back (Move ("bar", id2, data));

  UpdateState (10, moves);
  ExpectNumberOfChannels (1);
  auto h = ExpectChannel (id1);
  ASSERT_EQ (h->GetMetadata ().participants_size (), 2);
  EXPECT_EQ (h->GetMetadata ().participants (0).name (), "foo");
  EXPECT_EQ (h->GetMetadata ().participants (0).address (), "a");
  EXPECT_EQ (h->GetMetadata ().participants (1).name (), "bar");
  EXPECT_EQ (h->GetMetadata ().participants (1).address (), "b");
  EXPECT_FALSE (h->HasDispute ());

  proto::BoardState state;
  CHECK (state.ParseFromString (h->GetState ()));
  EXPECT_TRUE (state.has_turn ());
  EXPECT_EQ (state.turn (), 0);
}

/* ************************************************************************** */

using AbortChannelTests = StateUpdateTests;

TEST_F (AbortChannelTests, Malformed)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  tbl.CreateNew (existing)->MutableMetadata ().add_participants ();

  const auto txid = xaya::SHA256::Hash ("bar");

  std::vector<Json::Value> moves;
  for (const std::string& create : {"42", "null", "{}",
                                    R"({"id": "00"})",
                                    R"({"id": 100})",
                                    R"({"id": "00", "x": 5})"})
    {
      Json::Value data(Json::objectValue);
      data["a"] = ParseJson (create);
      moves.push_back (Move ("foo", txid, data));
    }
  UpdateState (10, moves);

  ExpectNumberOfChannels (1);
  ExpectChannel (existing);
}

TEST_F (AbortChannelTests, NonExistantChannel)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  tbl.CreateNew (existing)->MutableMetadata ().add_participants ();

  const auto txid = xaya::SHA256::Hash ("bar");
  Json::Value data (Json::objectValue);
  data["a"] = Json::Value (Json::objectValue);
  data["a"]["id"] = txid.ToHex ();
  UpdateState (10, {Move ("foo", txid, data)});

  ExpectNumberOfChannels (1);
  ExpectChannel (existing);
}

TEST_F (AbortChannelTests, AlreadyTwoParticipants)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  auto h = tbl.CreateNew (existing);
  h->MutableMetadata ().add_participants ()->set_name ("foo");
  h->MutableMetadata ().add_participants ()->set_name ("bar");
  h.reset ();

  const auto txid = xaya::SHA256::Hash ("bar");
  Json::Value data (Json::objectValue);
  data["a"] = Json::Value (Json::objectValue);
  data["a"]["id"] = existing.ToHex ();
  UpdateState (10, {Move ("baz", txid, data)});

  ExpectNumberOfChannels (1);
  ExpectChannel (existing);
}

TEST_F (AbortChannelTests, DifferentName)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  auto h = tbl.CreateNew (existing);
  h->MutableMetadata ().add_participants ()->set_name ("foo");
  h.reset ();

  const auto txid = xaya::SHA256::Hash ("bar");
  Json::Value data (Json::objectValue);
  data["a"] = Json::Value (Json::objectValue);
  data["a"]["id"] = existing.ToHex ();
  UpdateState (10, {Move ("bar", txid, data)});

  ExpectNumberOfChannels (1);
  ExpectChannel (existing);
}

TEST_F (AbortChannelTests, SuccessfulAbort)
{
  const auto existing = xaya::SHA256::Hash ("existing channel");
  tbl.CreateNew (existing);

  const auto id1 = xaya::SHA256::Hash ("foo");
  const auto id2 = xaya::SHA256::Hash ("bar");

  std::vector<Json::Value> moves;
  moves.push_back (Move ("foo", id1, ParseJson (R"({"c": {"addr": "a"}})")));

  Json::Value data(Json::objectValue);
  data["a"] = Json::Value (Json::objectValue);
  data["a"]["id"] = id1.ToHex ();
  moves.push_back (Move ("foo", id2, data));

  UpdateState (10, moves);
  ExpectNumberOfChannels (1);
  ExpectChannel (existing);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
