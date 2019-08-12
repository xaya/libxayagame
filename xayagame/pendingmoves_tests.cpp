// Copyright (C) 2019 The Xaya developers
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
    data["confirmed"] = GetConfirmedState ();
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
 * Constructs a block JSON for the given list of moves.
 */
Json::Value
BlockJson (const std::vector<Json::Value>& moves)
{
  Json::Value mvJson(Json::arrayValue);
  for (const auto& mv : moves)
    mvJson.append (mv);

  Json::Value res(Json::objectValue);
  res["moves"] = mvJson;

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
  proc.ProcessMove ("state", MoveJson ("foo", "bar"));
  proc.ProcessMove ("state", MoveJson ("foo", "baz"));
  proc.ProcessMove ("state", MoveJson ("foo", "bar"));
  proc.ProcessMove ("state", MoveJson ("abc", "def"));
  proc.ProcessMove ("state", MoveJson ("abc", "def"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "state",
    "names":
      {
        "abc": ["def"],
        "foo": ["bar", "baz"]
      }
  })"));
}

TEST_F (PendingMovesTests, AttachedBlock)
{
  proc.ProcessMove ("old", MoveJson ("foo", "c"));
  proc.ProcessMove ("old", MoveJson ("foo", "b"));
  proc.ProcessMove ("old", MoveJson ("foo", "a"));
  proc.ProcessMove ("old", MoveJson ("bar", "x"));
  proc.ProcessMove ("old", MoveJson ("baz", "y"));

  SetMempool ({"b", "c", "y", "z"});
  proc.ProcessAttachedBlock ("new");

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "new",
    "names":
      {
        "foo": ["b", "c"],
        "baz": ["y"]
      }
  })"));
}

TEST_F (PendingMovesTests, DetachedBlock)
{
  proc.ProcessMove ("new", MoveJson ("foo", "b"));
  proc.ProcessMove ("new", MoveJson ("bar", "x"));

  SetMempool ({"a", "b", "x", "y", "z"});
  proc.ProcessDetachedBlock ("old", BlockJson (
    {
      MoveJson ("foo", "a"),
      MoveJson ("baz", "y"),
    }));

  /* This should be ignored, as we have it already.  */
  proc.ProcessMove ("old", MoveJson ("foo", "a"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "old",
    "names":
      {
        "foo": ["a", "b"],
        "bar": ["x"],
        "baz": ["y"]
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

  proc.ProcessMove ("new 1", MoveJson ("foo", "b"));
  proc.ProcessMove ("new 1", MoveJson ("bar", "x"));

  SetMempool ({"a", "b", "x", "y"});
  proc.ProcessDetachedBlock ("old", BlockJson (
    {
      MoveJson ("foo", "a"),
      MoveJson ("baz", "y"),
    }));

  proc.ProcessMove ("old", MoveJson ("foo", "a"));
  proc.ProcessMove ("baz", MoveJson ("baz", "y"));

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "old",
    "names":
      {
        "foo": ["a", "b"],
        "bar": ["x"],
        "baz": ["y"]
      }
  })"));

  SetMempool ({});
  proc.ProcessAttachedBlock ("new 2");

  EXPECT_EQ (proc.ToJson (), ParseJson (R"({
    "confirmed": "new 2",
    "names": {}
  })"));
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
