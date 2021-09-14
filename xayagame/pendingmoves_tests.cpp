// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pendingmoves.hpp"

#include "testutils.hpp"

#include <xayautil/hash.hpp>

#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace xaya
{
namespace
{

using testing::Return;

/**
 * A simple implementation of a pending move processor.  Moves are expected
 * to be just strings, and we keep track of an ordered list of those strings
 * for each name (as a JSON array).
 */
class MessageArrayPendingMoves : public PendingMoveProcessor
{

private:

  /**
   * The current data as JSON object.  It has two fields:  The "names"
   * field holds an object mapping names to their arrays, and the "confirmed"
   * field stores the current game-state string.  The latter is useful to
   * test exposure of the confirmed state to the pending processor.
   */
  Json::Value data;

protected:

  void
  Clear () override
  {
    data = Json::Value (Json::objectValue);
    data["names"] = Json::Value (Json::objectValue);
  }

  void
  AddPendingMove (const Json::Value& mv) override
  {
    const std::string name = mv["name"].asString ();
    const std::string msg = mv["move"].asString ();

    auto& names = data["names"];
    CHECK (names.isObject ());
    if (!names.isMember (name))
      names[name] = Json::Value (Json::arrayValue);

    names[name].append (msg);

    data["confirmed"] = GetConfirmedState ();
    data["height"] = GetConfirmedBlock ()["height"];
  }

public:

  MessageArrayPendingMoves ()
    : data(Json::objectValue)
  {
    data["names"] = Json::Value (Json::objectValue);
  }

  Json::Value
  ToJson () const override
  {
    return data;
  }

};

/**
 * Constructs a move JSON for the given name and value.  The txid is computed
 * by hashing the value.
 */
Json::Value
MoveJson (const std::string& nm, const std::string& val)
{
  Json::Value res(Json::objectValue);
  res["txid"] = SHA256::Hash (val).ToHex ();
  res["name"] = nm;
  res["move"] = val;

  return res;
}

/**
 * Constructs a move JSON array for multiple moves supposed to be triggered
 * by a single transaction.  A dummy value is passed in which gets hashed
 * to produce the txid (which will be the same for all moves, as would be
 * the case for a real array-move situation).
 */
Json::Value
MoveJson (const std::string& txidPreimage,
          const std::vector<std::pair<std::string, std::string>>& moves)
{
  const std::string txidHex = SHA256::Hash (txidPreimage).ToHex ();

  Json::Value res(Json::arrayValue);
  for (const auto& entry : moves)
    {
      auto cur = MoveJson (entry.first, entry.second);
      cur["txid"] = txidHex;
      res.append (cur);
    }

  return res;
}

/**
 * Constructs a block JSON for the given list of moves.
 */
Json::Value
BlockJson (const unsigned height, const std::vector<Json::Value>& moves)
{
  Json::Value mvJson(Json::arrayValue);
  for (const auto& mv : moves)
    mvJson.append (mv);

  std::ostringstream hash;
  hash << "block " << height;
  std::ostringstream parent;
  parent << "block " << (height - 1);

  Json::Value data(Json::objectValue);
  data["height"] = static_cast<int> (height);
  data["hash"] = hash.str ();
  data["parent"] = parent.str ();

  Json::Value res(Json::objectValue);
  res["moves"] = mvJson;
  res["block"] = data;

  return res;
}

class PendingMovesTests : public testing::Test
{

private:

  HttpRpcServer<MockXayaRpcServer> mockXayaServer;

protected:

  MessageArrayPendingMoves proc;

  PendingMovesTests ()
  {
    proc.InitialiseGameContext (Chain::MAIN, "game id",
                                &mockXayaServer.GetClient ());
    SetMempool ({});
  }

  /**
   * Sets the mempool to return from the mock server.  The txid's are created
   * by hashing the provided strings (corresponding to "value" in MoveJson).
   */
  void
  SetMempool (const std::vector<std::string>& values)
  {
    Json::Value txids(Json::arrayValue);
    for (const auto& v : values)
      txids.append (SHA256::Hash (v).ToHex ());

    EXPECT_CALL (*mockXayaServer, getrawmempool ())
        .WillRepeatedly (Return (txids));
  }

};

/* ************************************************************************** */

TEST_F (PendingMovesTests, AddingMoves)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));

  proc.ProcessTx ("state", MoveJson ("foo", "bar"));
  proc.ProcessTx ("state", MoveJson ("foo", "baz"));
  proc.ProcessTx ("state", MoveJson ("foo", "bar"));
  proc.ProcessTx ("state", MoveJson ("abc", "def"));
  proc.ProcessTx ("state", MoveJson ("abc", "def"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "state",
    "height": 10,
    "names":
      {
        "abc": ["def"],
        "foo": ["bar", "baz"]
      }
  })"));
}

TEST_F (PendingMovesTests, AttachedBlock)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));

  proc.ProcessTx ("old", MoveJson ("foo", "c"));
  proc.ProcessTx ("old", MoveJson ("foo", "b"));
  proc.ProcessTx ("old", MoveJson ("foo", "a"));
  proc.ProcessTx ("old", MoveJson ("bar", "x"));
  proc.ProcessTx ("old", MoveJson ("baz", "y"));

  SetMempool ({"b", "c", "y", "z"});
  proc.ProcessAttachedBlock ("new", BlockJson (11, {}));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "new",
    "height": 11,
    "names":
      {
        "foo": ["b", "c"],
        "baz": ["y"]
      }
  })"));
}

TEST_F (PendingMovesTests, DetachedBlock)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));
  proc.ProcessAttachedBlock ("", BlockJson (11, {}));

  proc.ProcessTx ("new", MoveJson ("foo", "b"));
  proc.ProcessTx ("new", MoveJson ("bar", "x"));

  SetMempool ({"a", "b", "x", "y", "z"});
  proc.ProcessDetachedBlock ("old", BlockJson (11,
    {
      MoveJson ("foo", "a"),
      MoveJson ("baz", "y"),
    }));

  /* This should be ignored, as we have it already.  */
  proc.ProcessTx ("old", MoveJson ("foo", "a"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "old",
    "height": 10,
    "names":
      {
        "foo": ["a", "b"],
        "bar": ["x"],
        "baz": ["y"]
      }
  })"));
}

TEST_F (PendingMovesTests, ArrayMove)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));

  proc.ProcessTx ("old", ParseJson ("[]"));
  proc.ProcessTx ("old", MoveJson ("tx1", {{"foo", "c"}, {"foo", "b"}}));
  proc.ProcessTx ("old", MoveJson ("tx2", {{"foo", "a"}, {"bar", "x"}}));
  proc.ProcessTx ("old", MoveJson ("baz", "y"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "old",
    "height": 10,
    "names":
      {
        "bar": ["x"],
        "baz": ["y"],
        "foo": ["c", "b", "a"]
      }
  })"));

  SetMempool ({"tx1", "y"});
  proc.ProcessAttachedBlock ("new", BlockJson (11, {}));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "new",
    "height": 11,
    "names":
      {
        "baz": ["y"],
        "foo": ["c", "b"]
      }
  })"));
}

TEST_F (PendingMovesTests, ReplacedTransaction)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));

  proc.ProcessTx ("state", MoveJson ("tx1", {{"foo", "a"}, {"foo", "b"}}));
  proc.ProcessTx ("state", MoveJson ("tx2", {{"bar", "x"}}));

  /* Since replacing the transaction later on calls Reset, make sure the
     mempool matches up with the transactions.  */
  SetMempool ({"tx1", "tx2"});

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "state",
    "height": 10,
    "names":
      {
        "bar": ["x"],
        "foo": ["a", "b"]
      }
  })"));

  proc.ProcessTx ("state", MoveJson ("tx1", {{"foo", "1"}, {"foo", "2"}}));
  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "state",
    "height": 10,
    "names":
      {
        "bar": ["x"],
        "foo": ["1", "2"]
      }
  })"));
}

TEST_F (PendingMovesTests, OneBlockReorg)
{
  /* This test verifies what happens in the "typical" situation of a
     one-block-reorg (orphan block), assuming that notifications are sent
     in order (as they would if one socket is used for ZMQ blocks and
     pending moves).  While this should be covered by the tests before,
     it makes sense to verify this important situation also explicitly.  */

  proc.ProcessAttachedBlock ("", BlockJson (10, {}));
  proc.ProcessAttachedBlock ("", BlockJson (11, {}));

  proc.ProcessTx ("new 1", MoveJson ("foo", "b"));
  proc.ProcessTx ("new 1", MoveJson ("bar", "x"));

  SetMempool ({"a", "b", "x", "y"});
  proc.ProcessDetachedBlock ("old", BlockJson (11,
    {
      MoveJson ("foo", "a"),
      MoveJson ("baz", "y"),
    }));

  proc.ProcessTx ("old", MoveJson ("foo", "a"));
  proc.ProcessTx ("baz", MoveJson ("baz", "y"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "old",
    "height": 10,
    "names":
      {
        "foo": ["a", "b"],
        "bar": ["x"],
        "baz": ["y"]
      }
  })"));

  SetMempool ({});
  proc.ProcessAttachedBlock ("new 2", BlockJson (11, {}));
  proc.ProcessTx ("new 2", MoveJson ("foo", "new"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "new 2",
    "height": 11,
    "names":
      {
        "foo": ["new"]
      }
  })"));
}

TEST_F (PendingMovesTests, AttachedBlockQueueMismatch)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));
  proc.ProcessAttachedBlock ("", BlockJson (11, {}));

  /* We attach a block that mismatches with the current block queue.  It will
     still be used fine to process new pendings, but when we detach it, the
     queue will be empty.  */

  proc.ProcessAttachedBlock ("new", BlockJson (15, {}));
  proc.ProcessTx ("new", MoveJson ("foo", "a"));
  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "new",
    "height": 15,
    "names":
      {
        "foo": ["a"]
      }
  })"));

  proc.ProcessDetachedBlock ("old", BlockJson (15, {}));
  proc.ProcessTx ("old", MoveJson ("foo", "a"));
  EXPECT_EQ (proc.ToJson (), ParseJson (R"({"names": {}})"));
}

TEST_F (PendingMovesTests, DetachedBlockQueueMismatch)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));
  proc.ProcessAttachedBlock ("", BlockJson (11, {}));

  proc.ProcessDetachedBlock ("old", BlockJson (15, {}));
  proc.ProcessTx ("old", MoveJson ("foo", "a"));
  EXPECT_EQ (proc.ToJson (), ParseJson (R"({"names": {}})"));
}

TEST_F (PendingMovesTests, BlockQueuePruning)
{
  unsigned i = 100;
  while (i < 1'000)
    {
      ++i;
      proc.ProcessAttachedBlock ("", BlockJson (i, {}));
    }

  /* Detaching some blocks is fine.  */
  for (; i > 950; --i)
    proc.ProcessDetachedBlock ("", BlockJson (i, {}));

  proc.ProcessTx ("state", MoveJson ("foo", "a"));
  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "state",
    "height": 950,
    "names":
      {
        "foo": ["a"]
      }
  })"));

  /* Detaching more blocks than the queue is long is not good, as the initial
     blocks have been pruned already.  */
  for (; i > 800; --i)
    proc.ProcessDetachedBlock ("", BlockJson (i, {}));

  proc.ProcessTx ("state", MoveJson ("foo", "a"));
  EXPECT_EQ (proc.ToJson (), ParseJson (R"({"names": {}})"));
}

TEST_F (PendingMovesTests, InvalidMoveData)
{
  proc.ProcessAttachedBlock ("", BlockJson (10, {}));

  EXPECT_DEATH (proc.ProcessTx ("state", "invalid"), "Invalid move JSON");
  EXPECT_DEATH (proc.ProcessTx ("state", ParseJson ("[\"invalid\"]")),
                "Invalid move JSON");

  auto arr = MoveJson ("tx", {{"foo", "c"}, {"foo", "b"}});
  arr[0]["txid"] = SHA256::Hash ("other").ToHex ();
  EXPECT_DEATH (proc.ProcessTx ("state", arr), "Txid mismatch");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
