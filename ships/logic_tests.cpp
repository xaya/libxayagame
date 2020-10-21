// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include "proto/boardstate.pb.h"
#include "testutils.hpp"

#include <gamechannel/database.hpp>
#include <gamechannel/proto/stateproof.pb.h>
#include <gamechannel/protoutils.hpp>
#include <gamechannel/signatures.hpp>
#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <google/protobuf/text_format.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

namespace ships
{

class StateUpdateTests : public InMemoryLogicFixture
{

protected:

  xaya::ChannelsTable tbl;

  StateUpdateTests ()
    : tbl(GetDb ())
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

    game.UpdateState (GetDb (), blockData);
  }

  /**
   * Expects that the number of open channels is the given one.  This can be
   * used to verify there are no unexpected channels.
   */
  void
  ExpectNumberOfChannels (const unsigned expected)
  {
    unsigned actual = 0;
    auto stmt = tbl.QueryAll ();
    while (stmt.Step ())
      ++actual;

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

  /**
   * Exposes UpdateStats for testing.
   */
  void
  UpdateStats (const xaya::proto::ChannelMetadata& meta, const int winner)
  {
    game.UpdateStats (GetDb (), meta, winner);
  }

  /**
   * Inserts a row into the game_stats table (to define pre-existing data
   * for testing updates to it).
   */
  void
  AddStatsRow (const std::string& name, const int won, const int lost)
  {
    auto stmt = GetDb ().Prepare (R"(
      INSERT INTO `game_stats`
        (`name`, `won`, `lost`) VALUES (?1, ?2, ?3)
    )");
    ShipsLogic::BindStringParam (*stmt, 1, name);
    CHECK_EQ (sqlite3_bind_int (*stmt, 2, won), SQLITE_OK);
    CHECK_EQ (sqlite3_bind_int (*stmt, 3, lost), SQLITE_OK);
    stmt.Execute ();
  }

  /**
   * Verifies that the game stats for the given name match the given values.
   */
  void
  ExpectStatsRow (const std::string& name, const int won, const int lost)
  {
    auto stmt = GetDb ().Prepare (R"(
      SELECT `won`, `lost`
        FROM `game_stats`
        WHERE `name` = ?1
    )");
    ShipsLogic::BindStringParam (*stmt, 1, name);

    CHECK (stmt.Step ()) << "No stats row for: " << name;
    EXPECT_EQ (sqlite3_column_int (*stmt, 0), won);
    EXPECT_EQ (sqlite3_column_int (*stmt, 1), lost);

    CHECK (!stmt.Step ());
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

/**
 * Returns a serialised state for the given text proto.
 */
xaya::BoardState
SerialisedState (const std::string& str)
{
  proto::BoardState state;
  CHECK (TextFormat::ParseFromString (str, &state));

  xaya::BoardState res;
  CHECK (state.SerializeToString (&res));

  return res;
}

/**
 * Returns a JSON dispute/resolution move object for the channel which has
 * a state proof for the state parsed from text and signed by the given
 * signatures.  Key should be either "r" or "d" to build resolutions or
 * disputes, respectively.
 */
Json::Value
BuildDisputeResolutionMove (const xaya::uint256& channelId,
                            const xaya::uint256& txid,
                            const std::string& key, const std::string& stateStr,
                            const std::vector<std::string>& signatures)
{
  xaya::proto::StateProof proof;
  auto* is = proof.mutable_initial_state ();
  *is->mutable_data () = SerialisedState (stateStr);
  for (const auto& sgn : signatures)
    is->add_signatures (sgn);

  Json::Value data(Json::objectValue);
  data[key] = Json::Value (Json::objectValue);
  data[key]["id"] = channelId.ToHex ();
  data[key]["state"] = xaya::ProtoToBase64 (proof);

  return Move ("xyz", txid, data);
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
  EXPECT_EQ (h->GetLatestState (), "");
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
  mockXayaServer->StopListening ();

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
  auto h = tbl.CreateNew (existing);
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ();
  h->Reinitialise (meta, "");
  h.reset ();

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
  auto h = tbl.CreateNew (existing);
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ();
  h->Reinitialise (meta, "");
  h.reset ();

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
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ()->set_name ("foo");
  meta.add_participants ()->set_name ("bar");
  h->Reinitialise (meta, SerialisedState ("turn: 0"));
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
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ()->set_name ("foo");
  h->Reinitialise (meta, "");
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
  CHECK (state.ParseFromString (h->GetLatestState ()));
  EXPECT_TRUE (state.has_turn ());
  EXPECT_EQ (state.turn (), 0);
}

/* ************************************************************************** */

using AbortChannelTests = StateUpdateTests;

TEST_F (AbortChannelTests, Malformed)
{
  const auto existing = xaya::SHA256::Hash ("foo");
  auto h = tbl.CreateNew (existing);
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ();
  h->Reinitialise (meta, "");
  h.reset ();

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
  auto h = tbl.CreateNew (existing);
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ();
  h->Reinitialise (meta, "");
  h.reset ();

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
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ()->set_name ("foo");
  meta.add_participants ()->set_name ("bar");
  h->Reinitialise (meta, SerialisedState ("turn: 0"));
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
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ()->set_name ("foo");
  h->Reinitialise (meta, "");
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
  auto h = tbl.CreateNew (existing);
  xaya::proto::ChannelMetadata meta;
  meta.add_participants ();
  h->Reinitialise (meta, "");
  h.reset ();

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

class CloseChannelTests : public StateUpdateTests
{

protected:

  xaya::proto::ChannelMetadata meta;

  /**
   * ID of the channel closed in tests (or not).  This channel is set up
   * with players "name 0" and "name 1".
   */
  const xaya::uint256 channelId = xaya::SHA256::Hash ("test channel");

  /** ID of a channel that should not be affected by any close.  */
  const xaya::uint256 otherId = xaya::SHA256::Hash ("other channel");

  /** Txid for use with the move.  */
  const xaya::uint256 txid = xaya::SHA256::Hash ("txid");

  CloseChannelTests ()
  {
    CHECK (TextFormat::ParseFromString (R"(
      participants:
        {
          name: "name 0"
          address: "addr 0"
        }
      participants:
        {
          name: "name 1"
          address: "addr 1"
        }
    )", &meta));

    auto h = tbl.CreateNew (channelId);
    h->Reinitialise (meta, SerialisedState ("turn: 0"));
    h.reset ();

    h = tbl.CreateNew (otherId);
    h->Reinitialise (meta, SerialisedState ("turn: 0"));
    h.reset ();
  }

  /**
   * Expects a signature validation call on the mock RPC server for a winner
   * statement on our channel ID, and returns that it is valid with the given
   * signing address.
   */
  void
  ExpectSignature (const proto::WinnerStatement& stmt, const std::string& sgn,
                   const std::string& addr)
  {
    Json::Value res(Json::objectValue);
    res["valid"] = true;
    res["address"] = addr;

    std::string data;
    CHECK (stmt.SerializeToString (&data));

    const std::string hashed
        = xaya::GetChannelSignatureMessage (channelId, meta,
                                            "winnerstatement", data);
    EXPECT_CALL (*mockXayaServer,
                 verifymessage ("", hashed, xaya::EncodeBase64 (sgn)))
        .WillOnce (Return (res));
  }

  /**
   * Returns a JSON "close" move object for our channel and based on the given
   * SignedData proto.
   */
  Json::Value
  CloseMove (const xaya::proto::SignedData& signedData)
  {
    Json::Value data(Json::objectValue);
    data["w"] = Json::Value (Json::objectValue);
    data["w"]["id"] = channelId.ToHex ();
    data["w"]["stmt"] = xaya::ProtoToBase64 (signedData);

    return Move ("xyz", txid, data);
  }

  /**
   * Builds up a "close" move with the given WinnerStatement and signatures.
   */
  Json::Value
  CloseMove (const proto::WinnerStatement& stmt,
             const std::vector<std::string>& signatures)
  {
    xaya::proto::SignedData signedData;
    CHECK (stmt.SerializeToString (signedData.mutable_data ()));

    for (const auto& sgn : signatures)
      signedData.add_signatures (sgn);

    return CloseMove (signedData);
  }

};

TEST_F (CloseChannelTests, UpdateStats)
{
  AddStatsRow ("foo", 10, 5);
  AddStatsRow ("bar", 1, 2);
  ExpectStatsRow ("foo", 10, 5);
  ExpectStatsRow ("bar", 1, 2);

  xaya::proto::ChannelMetadata meta;
  meta.add_participants ()->set_name ("foo");
  meta.add_participants ()->set_name ("baz");

  UpdateStats (meta, 0);
  ExpectStatsRow ("foo", 11, 5);
  ExpectStatsRow ("bar", 1, 2);
  ExpectStatsRow ("baz", 0, 1);

  UpdateStats (meta, 1);
  ExpectStatsRow ("foo", 11, 6);
  ExpectStatsRow ("bar", 1, 2);
  ExpectStatsRow ("baz", 1, 1);
}

TEST_F (CloseChannelTests, Malformed)
{
  std::vector<Json::Value> moves;
  for (const std::string& create : {"42", "null", "{}",
                                    R"({"id": "00"})",
                                    R"({"id": 100, "stmt": ""})",
                                    R"({"id": "00", "stmt": ""})",
                                    R"({"id": "00", "stmt": "", "x": 5})"})
    {
      Json::Value data(Json::objectValue);
      data["w"] = ParseJson (create);
      moves.push_back (Move ("xyz", txid, data));
    }
  UpdateState (10, moves);

  ExpectNumberOfChannels (2);
  ExpectChannel (channelId);
  ExpectChannel (otherId);
}

TEST_F (CloseChannelTests, InvalidStmtData)
{
  Json::Value data(Json::objectValue);
  data["w"] = Json::Value (Json::objectValue);
  data["w"]["id"] = channelId.ToHex ();
  data["w"]["stmt"] = "invalid base64";
  UpdateState (10, {Move ("xyz", txid, data)});

  data["w"]["stmt"] = xaya::EncodeBase64 ("invalid proto");
  UpdateState (11, {Move ("xyz", txid, data)});

  ExpectNumberOfChannels (2);
  ExpectChannel (channelId);
  ExpectChannel (otherId);
}

TEST_F (CloseChannelTests, NonExistantChannel)
{
  Json::Value data(Json::objectValue);
  data["w"] = Json::Value (Json::objectValue);
  data["w"]["id"] = xaya::SHA256::Hash ("channel does not exist").ToHex ();
  data["w"]["stmt"] = "";
  UpdateState (10, {Move ("xyz", txid, data)});

  ExpectNumberOfChannels (2);
  ExpectChannel (channelId);
  ExpectChannel (otherId);
}

TEST_F (CloseChannelTests, WrongNumberOfParticipants)
{
  auto h = ExpectChannel (channelId);
  xaya::proto::ChannelMetadata meta = h->GetMetadata ();
  meta.mutable_participants ()->RemoveLast ();
  meta.set_reinit ("init 2");
  h->Reinitialise (meta, "");
  h.reset ();

  auto mv = CloseMove (xaya::proto::SignedData ());
  UpdateState (10, {mv});

  ExpectNumberOfChannels (2);
  ExpectChannel (channelId);
  ExpectChannel (otherId);
}

TEST_F (CloseChannelTests, InvalidWinnerStatement)
{
  proto::WinnerStatement stmt;
  stmt.set_winner (0);

  auto mv = CloseMove (stmt, {});
  UpdateState (10, {mv});

  ExpectNumberOfChannels (2);
  ExpectChannel (channelId);
  ExpectChannel (otherId);
}

TEST_F (CloseChannelTests, Valid)
{
  proto::WinnerStatement stmt;
  stmt.set_winner (1);

  ExpectSignature (stmt, "sgn 0", "addr 0");

  auto mv = CloseMove (stmt, {"sgn 0"});
  UpdateState (10, {mv});

  ExpectNumberOfChannels (1);
  ExpectChannel (otherId);
  ExpectStatsRow ("name 0", 0, 1);
  ExpectStatsRow ("name 1", 1, 0);
}

/* ************************************************************************** */

class DisputeResolutionTests : public StateUpdateTests
{

protected:

  /**
   * ID of the channel closed in tests (or not).  This channel is set up
   * with players "name 0" and "name 1".
   */
  const xaya::uint256 channelId = xaya::SHA256::Hash ("test channel");

  /** Txid for use with the move.  */
  const xaya::uint256 txid = xaya::SHA256::Hash ("txid");

  DisputeResolutionTests ()
  {
    auto h = tbl.CreateNew (channelId);

    xaya::proto::ChannelMetadata meta;
    CHECK (TextFormat::ParseFromString (R"(
      participants:
        {
          name: "name 0"
          address: "addr 0"
        }
      participants:
        {
          name: "name 1"
          address: "addr 1"
        }
    )", &meta));

    h->Reinitialise (meta, SerialisedState ("turn: 0"));
    h.reset ();

    Json::Value signatureOk(Json::objectValue);
    signatureOk["valid"] = true;
    signatureOk["address"] = "addr 0";
    EXPECT_CALL (*mockXayaServer,
                 verifymessage ("", _, xaya::EncodeBase64 ("sgn 0")))
        .WillRepeatedly (Return (signatureOk));

    signatureOk["address"] = "addr 1";
    EXPECT_CALL (*mockXayaServer,
                 verifymessage ("", _, xaya::EncodeBase64 ("sgn 1")))
        .WillRepeatedly (Return (signatureOk));

    /* Explicitly add stats rows so we can use ExpectStatsRow even if there
       were no changes.  */
    AddStatsRow ("name 0", 0, 0);
    AddStatsRow ("name 1", 0, 0);
  }

  Json::Value
  BuildMove (const std::string& key, const std::string& stateStr,
             const std::vector<std::string>& signatures)
  {
    return BuildDisputeResolutionMove (channelId, txid,
                                       key, stateStr, signatures);
  }

};

TEST_F (DisputeResolutionTests, ExpiringDisputes)
{
  ExpectChannel (channelId)->SetDisputeHeight (100);

  UpdateState (109, {});
  ExpectNumberOfChannels (1);
  ExpectChannel (channelId);
  ExpectStatsRow ("name 0", 0, 0);
  ExpectStatsRow ("name 1", 0, 0);

  UpdateState (110, {});
  ExpectNumberOfChannels (0);
  ExpectStatsRow ("name 0", 0, 1);
  ExpectStatsRow ("name 1", 1, 0);
}

TEST_F (DisputeResolutionTests, Malformed)
{
  std::vector<Json::Value> moves;
  for (const std::string& str : {"42", "null", "{}",
                                  R"({"id": "00"})",
                                  R"({"id": 100, "state": ""})",
                                  R"({"id": "00", "state": ""})",
                                  R"({"id": "00", "state": "", "x": 5})"})
    {
      Json::Value data(Json::objectValue);
      data["r"] = ParseJson (str);
      moves.push_back (Move ("xyz", txid, data));

      data.clear ();
      data["d"] = ParseJson (str);
      moves.push_back (Move ("xyz", txid, data));
    }
  UpdateState (10, moves);

  ExpectNumberOfChannels (1);
  EXPECT_FALSE (ExpectChannel (channelId)->HasDispute ());
}

TEST_F (DisputeResolutionTests, InvalidStateData)
{
  Json::Value data(Json::objectValue);
  data["d"] = Json::Value (Json::objectValue);
  data["d"]["id"] = channelId.ToHex ();
  data["d"]["state"] = "invalid base64";
  UpdateState (10, {Move ("xyz", txid, data)});

  data["d"]["state"] = xaya::EncodeBase64 ("invalid proto");
  UpdateState (11, {Move ("xyz", txid, data)});

  ExpectNumberOfChannels (1);
  EXPECT_FALSE (ExpectChannel (channelId)->HasDispute ());
}

TEST_F (DisputeResolutionTests, NonExistantChannel)
{
  auto mv = BuildMove ("d", "turn: 1 winner: 0", {"sgn 0", "sgn 1"});
  mv["move"]["d"]["id"] = xaya::SHA256::Hash ("invalid channel").ToHex ();
  UpdateState (10, {mv});

  ExpectNumberOfChannels (1);
  EXPECT_FALSE (ExpectChannel (channelId)->HasDispute ());
}

TEST_F (DisputeResolutionTests, WrongNumberOfParticipants)
{
  auto h = ExpectChannel (channelId);
  xaya::proto::ChannelMetadata meta = h->GetMetadata ();
  meta.mutable_participants ()->RemoveLast ();
  meta.set_reinit ("init 2");
  h->Reinitialise (meta, h->GetLatestState ());
  h.reset ();

  UpdateState (10, {BuildMove ("d", "turn: 1 winner: 0", {"sgn 0", "sgn 1"})});

  ExpectNumberOfChannels (1);
  EXPECT_FALSE (ExpectChannel (channelId)->HasDispute ());
}

TEST_F (DisputeResolutionTests, InvalidStateProof)
{
  UpdateState (10, {BuildMove ("d", "turn: 1 winner: 0", {})});

  ExpectNumberOfChannels (1);
  EXPECT_FALSE (ExpectChannel (channelId)->HasDispute ());
}

TEST_F (DisputeResolutionTests, ValidDispute)
{
  UpdateState (10, {BuildMove ("d", "turn: 0", {})});

  ExpectNumberOfChannels (1);
  auto h = ExpectChannel (channelId);
  ASSERT_TRUE (h->HasDispute ());
  EXPECT_EQ (h->GetDisputeHeight (), 10);
}

TEST_F (DisputeResolutionTests, ValidResolution)
{
  ExpectChannel (channelId)->SetDisputeHeight (100);

  UpdateState (110, {BuildMove ("r", "turn: 1 winner: 0", {"sgn 0", "sgn 1"})});

  ExpectNumberOfChannels (1);
  ASSERT_FALSE (ExpectChannel (channelId)->HasDispute ());
}

/* ************************************************************************** */

} // anonymous namespace

class PendingTests : public InMemoryLogicFixture
{

private:

  ShipsPending proc;

protected:

  xaya::proto::ChannelMetadata meta;

  xaya::ChannelsTable tbl;

  PendingTests ()
    : proc(game), tbl(GetDb ())
  {
    proc.InitialiseGameContext (xaya::Chain::MAIN, "xs",
                                &mockXayaServer.GetClient ());

    CHECK (TextFormat::ParseFromString (R"(
      participants:
        {
          name: "name 0"
          address: "addr 0"
        }
      participants:
        {
          name: "name 1"
          address: "addr 1"
        }
    )", &meta));

    Json::Value signatureOk(Json::objectValue);
    signatureOk["valid"] = true;
    signatureOk["address"] = "addr 0";
    EXPECT_CALL (*mockXayaServer,
                 verifymessage ("", _, xaya::EncodeBase64 ("sgn 0")))
        .WillRepeatedly (Return (signatureOk));

    signatureOk["address"] = "addr 1";
    EXPECT_CALL (*mockXayaServer,
                 verifymessage ("", _, xaya::EncodeBase64 ("sgn 1")))
        .WillRepeatedly (Return (signatureOk));
  }

  /**
   * Submits a pending move to the processor.
   */
  void
  AddPendingMove (const Json::Value& mv)
  {
    proc.AddPendingMoveUnsafe (GetDb (), mv);
  }

  /**
   * Expects that the pending state has exactly the given channels (by ID).
   * We do not care about the data for each channel, as this test is mostly
   * about move parsing and forwarding of data.
   */
  void
  ExpectPendingChannels (const std::set<xaya::uint256>& expected)
  {
    const auto actualJson = proc.ToJson ()["channels"];
    ASSERT_TRUE (actualJson.isObject ());

    std::set<xaya::uint256> actual;
    for (auto it = actualJson.begin (); it != actualJson.end (); ++it)
      {
        xaya::uint256 txid;
        ASSERT_TRUE (txid.FromHex (it.key ().asString ()));
        actual.insert (txid);
      }
    ASSERT_EQ (actual.size (), actualJson.size ());

    EXPECT_EQ (actual, expected);
  }

};

namespace
{

TEST_F (PendingTests, StatesProcessed)
{
  const auto cid1 = xaya::SHA256::Hash ("channel 1");
  auto h = tbl.CreateNew (cid1);
  h->Reinitialise (meta, SerialisedState ("turn: 0"));
  h.reset ();

  const auto cid2 = xaya::SHA256::Hash ("channel 2");
  h = tbl.CreateNew (cid2);
  h->Reinitialise (meta, SerialisedState ("turn: 0"));
  h.reset ();

  const auto mv1 = BuildDisputeResolutionMove (
      cid1, xaya::SHA256::Hash ("tx 1"), "d",
      R"(
        turn: 1
        position_hashes: "foo"
      )", {"sgn 0", "sgn 1"});
  AddPendingMove (mv1);

  const auto mv2 = BuildDisputeResolutionMove (
      cid2, xaya::SHA256::Hash ("tx 2"), "r",
      R"(
        turn: 0
        winner: 1
      )", {"sgn 0", "sgn 1"});
  AddPendingMove (mv2);

  ExpectPendingChannels ({cid1, cid2});
}

TEST_F (PendingTests, NonObjectMove)
{
  const auto cid = xaya::SHA256::Hash ("channel");
  auto h = tbl.CreateNew (cid);
  h->Reinitialise (meta, SerialisedState ("turn: 0"));
  h.reset ();

  AddPendingMove (ParseJson (R"(
    {
      "move": 42
    }
  )"));

  ExpectPendingChannels ({});
}

TEST_F (PendingTests, NonExistantChannel)
{
  const auto cid = xaya::SHA256::Hash ("channel");
  auto h = tbl.CreateNew (cid);
  h->Reinitialise (meta, SerialisedState ("turn: 0"));
  h.reset ();

  const auto wrongCid = xaya::SHA256::Hash ("other channel");
  const auto mv = BuildDisputeResolutionMove (
      wrongCid, xaya::SHA256::Hash ("tx"), "r",
      R"(
        turn: 0
      )", {"sgn 0", "sgn 1"});
  AddPendingMove (mv);

  ExpectPendingChannels ({});
}

TEST_F (PendingTests, InvalidStateProof)
{
  const auto cid = xaya::SHA256::Hash ("channel");
  auto h = tbl.CreateNew (cid);
  h->Reinitialise (meta, SerialisedState ("turn: 0"));
  h.reset ();

  auto mv = ParseJson (R"(
    {
      "move":
        {
          "d":
            {
              "state": "invalid base64 proto"
            }
        }
    }
  )");
  mv["move"]["d"]["id"] = cid.ToHex ();
  AddPendingMove (mv);

  ExpectPendingChannels ({});
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
