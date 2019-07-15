// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamestatejson.hpp"

#include "proto/boardstate.pb.h"
#include "testutils.hpp"

#include <gamechannel/protoutils.hpp>
#include <xayautil/hash.hpp>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <sstream>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;

namespace ships
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

class GameStateJsonTests : public InMemoryLogicFixture
{

protected:

  xaya::ChannelsTable tbl;
  GameStateJson gsj;

  GameStateJsonTests ()
    : tbl(game), gsj(game)
  {}

};

TEST_F (GameStateJsonTests, GameStats)
{
  CHECK_EQ (sqlite3_exec (GetDb (), R"(
    INSERT INTO `game_stats`
      (`name`, `won`, `lost`) VALUES ('foo', 10, 2), ('bar', 5, 5)
  )", nullptr, nullptr, nullptr), SQLITE_OK);

  EXPECT_EQ (gsj.GetFullJson (), ParseJson (R"(
    {
      "channels": {},
      "gamestats":
        {
          "foo": {"won": 10, "lost": 2},
          "bar": {"won": 5, "lost": 5}
        }
    }
  )"));
}

TEST_F (GameStateJsonTests, OneParticipantChannel)
{
  const auto id = xaya::SHA256::Hash ("channel");
  auto h = tbl.CreateNew (id);
  xaya::proto::ChannelMetadata meta;
  CHECK (TextFormat::ParseFromString (R"(
    participants:
      {
        name: "only me"
        address: "addr"
      }
  )", &meta));
  h->Reinitialise (meta, "");
  h.reset ();

  auto expected = ParseJson (R"(
    {
      "channels": {},
      "gamestats": {}
    }
  )");
  expected["channels"][id.ToHex ()] = ParseJson (R"(
    {
      "meta":
        {
          "reinit": "",
          "participants": [{"name": "only me", "address": "addr"}]
        },
      "state":
        {
          "parsed":
            {
              "phase": "single participant"
            },
          "base64": "",
          "whoseturn": null,
          "turncount": 0
        },
      "reinit":
        {
          "parsed":
            {
              "phase": "single participant"
            },
          "base64": "",
          "whoseturn": null,
          "turncount": 0
        }
    }
  )");
  expected["channels"][id.ToHex ()]["id"] = id.ToHex ();

  auto actual = gsj.GetFullJson ();
  actual["channels"][id.ToHex ()]["meta"].removeMember ("proto");
  actual["channels"][id.ToHex ()]["state"].removeMember ("proof");
  EXPECT_EQ (actual, expected);
}

TEST_F (GameStateJsonTests, TwoParticipantChannel)
{
  const auto id = xaya::SHA256::Hash ("channel");
  auto h = tbl.CreateNew (id);
  xaya::proto::ChannelMetadata meta;
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

  proto::BoardState state;
  state.set_turn (0);

  std::string serialised;
  CHECK (state.SerializeToString (&serialised));

  h->Reinitialise (meta, serialised);
  h.reset ();

  const auto actual = gsj.GetFullJson ();
  const auto& stateJson = actual["channels"][id.ToHex ()]["state"];
  ASSERT_TRUE (stateJson.isObject ());

  EXPECT_EQ (stateJson["whoseturn"].asInt (), 0);
  EXPECT_EQ (stateJson["turncount"].asInt (), 1);
  EXPECT_EQ (stateJson["parsed"]["phase"].asString (), "first commitment");

  proto::BoardState stateFromJson;
  ASSERT_TRUE (xaya::ProtoFromBase64 (stateJson["base64"].asString (),
                                      stateFromJson));
  EXPECT_TRUE (MessageDifferencer::Equals (state, stateFromJson));
}

} // anonymous namespace
} // namespace ships
