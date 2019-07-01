// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamestatejson.hpp"

#include "testgame.hpp"

#include <xayautil/hash.hpp>
#include <xayautil/uint256.hpp>

#include <gtest/gtest.h>

#include <google/protobuf/text_format.h>

#include <glog/logging.h>

#include <sstream>

using google::protobuf::TextFormat;

namespace xaya
{
namespace
{

Json::Value
ParseJson (const std::string& str)
{
  std::istringstream in(str);
  Json::Value res;
  in >> res;
  CHECK (in);

  return res;
}

class GameStateJsonTests : public TestGameFixture
{

protected:

  ChannelsTable tbl;

  /** Test channel set up with state (100, 2).   */
  const xaya::uint256 id1 = xaya::SHA256::Hash ("channel 1");

  /** Test channel set up with state (40, 10) and a dispute.  */
  const xaya::uint256 id2 = xaya::SHA256::Hash ("channel 2");

  GameStateJsonTests ()
    : tbl(game)
  {
    proto::ChannelMetadata meta;
    CHECK (TextFormat::ParseFromString (R"(
      participants:
        {
          name: "foo"
          address: "addr 1"
        }
      participants:
        {
          name: "bar"
          address: "addr 2"
        }
    )", &meta));

    auto h = tbl.CreateNew (id1);
    h->Reinitialise (meta, "100 2");
    h.reset ();

    h = tbl.CreateNew (id2);
    meta.mutable_participants (1)->set_name ("baz");
    h->SetDisputeHeight (55);
    h->Reinitialise (meta, "40 10");
    h.reset ();
  }

};

TEST_F (GameStateJsonTests, WithoutDispute)
{
  auto expected = ParseJson (R"(
    {
      "meta":
        {
          "participants":
            [
              {"name": "foo", "address": "addr 1"},
              {"name": "bar", "address": "addr 2"}
            ]
        },
      "state":
        {
          "data": {"count": 2, "number": 100},
          "turncount": 2,
          "whoseturn": null
        }
    }
  )");
  expected["id"] = id1.ToHex ();

  auto h = tbl.GetById (id1);
  EXPECT_EQ (ChannelToGameStateJson (*h, game.rules), expected);
}

TEST_F (GameStateJsonTests, WithDispute)
{
  auto expected = ParseJson (R"(
    {
      "disputeheight": 55,
      "meta":
        {
          "participants":
            [
              {"name": "foo", "address": "addr 1"},
              {"name": "baz", "address": "addr 2"}
            ]
        },
      "state":
        {
          "data": {"count": 10, "number": 40},
          "turncount": 10,
          "whoseturn": 0
        }
    }
  )");
  expected["id"] = id2.ToHex ();

  auto h = tbl.GetById (id2);
  EXPECT_EQ (ChannelToGameStateJson (*h, game.rules), expected);
}

TEST_F (GameStateJsonTests, AllChannels)
{
  Json::Value expected(Json::objectValue);

  auto h = tbl.GetById (id1);
  expected[h->GetId ().ToHex ()] = ChannelToGameStateJson (*h, game.rules);
  h = tbl.GetById (id2);
  expected[h->GetId ().ToHex ()] = ChannelToGameStateJson (*h, game.rules);

  EXPECT_EQ (AllChannelsGameStateJson (tbl, game.rules), expected);
}

} // anonymous namespace
} // namespace xaya
