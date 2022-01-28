// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelstatejson_tests.hpp"

#include "protoutils.hpp"

#include <xayautil/base64.hpp>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <glog/logging.h>

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;

namespace xaya
{

void
CheckChannelJson (Json::Value actual, const std::string& expected,
                  const uint256& id, const proto::ChannelMetadata& meta,
                  const BoardState& reinitState,
                  const BoardState& proofState)
{
  ASSERT_EQ (actual["id"].asString (), id.ToHex ());
  actual.removeMember ("id");

  /* Metadata serialisation is checked separately, so ignore it for testing
     the full channels (just to avoid the need for duplicating the code that
     handles the encoded fields).  */
  actual.removeMember ("meta");

  ASSERT_EQ (actual["reinit"]["base64"].asString (),
             EncodeBase64 (reinitState));
  actual["reinit"].removeMember ("base64");

  ASSERT_EQ (actual["state"]["base64"].asString (), EncodeBase64 (proofState));
  actual["state"].removeMember ("base64");

  proto::StateProof proof;
  ASSERT_TRUE (ProtoFromBase64 (actual["state"]["proof"].asString (), proof));
  ASSERT_EQ (proof.initial_state ().data (), proofState);
  actual["state"].removeMember ("proof");

  ASSERT_EQ (actual, ParseJson (expected));
}

ChannelStateJsonTests::ChannelStateJsonTests ()
{
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
  )", &meta1));

  meta2 = meta1;
  meta2.mutable_participants (1)->set_name ("baz");
  meta2.set_reinit ("reinit id");
}

namespace
{

TEST_F (ChannelStateJsonTests, ChannelMetadataToJson)
{
  auto actual = ChannelMetadataToJson (meta2);

  ASSERT_EQ (actual["reinit"], EncodeBase64 (meta2.reinit ()));
  actual.removeMember ("reinit");

  proto::ChannelMetadata actualMeta;
  ASSERT_TRUE (ProtoFromBase64 (actual["proto"].asString (), actualMeta));
  ASSERT_TRUE (MessageDifferencer::Equals (actualMeta, meta2));
  actual.removeMember ("proto");

  EXPECT_EQ (actual, ParseJson (R"({
    "participants":
      [
        {"name": "foo", "address": "addr 1"},
        {"name": "baz", "address": "addr 2"}
      ]
  })"));
}

TEST_F (ChannelStateJsonTests, BoardStateToJson)
{
  auto actual = BoardStateToJson (game.rules, id1, meta1, "10 5");

  ASSERT_EQ (actual["base64"].asString (), EncodeBase64 ("10 5"));
  actual.removeMember ("base64");

  EXPECT_EQ (actual, ParseJson (R"({
    "parsed": {"count": 5, "number": 10},
    "turncount": 5,
    "whoseturn": 0
  })"));
}

} // anonymous namespace
} // namespace xaya
