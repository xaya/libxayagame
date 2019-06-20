// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protoboard.hpp"

#include "boardrules.hpp"
#include "proto/metadata.pb.h"
#include "proto/protoboardtest.pb.h"

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
 * Parses a text-format TestBoardState proto and serialises it in wire format.
 */
BoardState
TextState (const std::string& str)
{
  proto::TestBoardState pb;
  CHECK (TextFormat::ParseFromString (str, &pb));

  std::string res;
  CHECK (pb.SerializeToString (&res));

  return res;
}

/**
 * Parses a text-format TestBoardMove proto and serialises it in wire format.
 */
BoardState
TextMove (const std::string& str)
{
  proto::TestBoardMove pb;
  CHECK (TextFormat::ParseFromString (str, &pb));

  std::string res;
  CHECK (pb.SerializeToString (&res));

  return res;
}

using SuperState = ProtoBoardState<proto::TestBoardState, proto::TestBoardMove>;

/**
 * Test implementation for the proto board state with "proper" rules.
 */
class TestState : public SuperState
{

protected:

  bool
  EqualsProto (const proto::TestBoardState& other) const override
  {
    if (GetState ().has_only_compare_this () && other.has_only_compare_this ())
      return GetState ().only_compare_this () == other.only_compare_this ();

    return SuperState::EqualsProto (other);
  }

  bool
  ApplyMoveProto (XayaRpcClient& rpc, const proto::TestBoardMove& mv,
                  proto::TestBoardState& newState) const override
  {
    if (!mv.has_msg ())
      return false;

    newState.Clear ();
    newState.set_msg (mv.msg ());
    return true;
  }

public:

  using SuperState::SuperState;

  bool
  IsValid () const override
  {
    CHECK (SuperState::IsValid ());
    return !GetState ().invalid ();
  }

  int
  WhoseTurn () const override
  {
    LOG (FATAL) << "Not implemented";
  }

  unsigned
  TurnCount () const override
  {
    LOG (FATAL) << "Not implemented";
  }

};

using TestRules = ProtoBoardRules<TestState>;

class ProtoBoardTests : public testing::Test
{

protected:

  /**
   * Fake instance of XayaRpcClient, which is just a disguised null pointer.
   * This is fine, as our board rules never need the RPC.
   */
  XayaRpcClient& rpc;

  /** Fake channel ID used in tests.  */
  const uint256 channelId = SHA256::Hash ("foo");

  /**
   * Metadata instance.  We actually verify that its address is correct
   * (i.e. it gets passed everywhere by reference) rather than the content.
   */
  proto::ChannelMetadata meta;

  TestRules rules;

  ProtoBoardTests ()
    : rpc(*static_cast<XayaRpcClient*> (nullptr))
  {}

  /**
   * Parses a board state and returns it as TestState pointer (instead of
   * the generic ParsedBoardState).
   */
  std::unique_ptr<TestState>
  ParseState (const BoardState& s)
  {
    auto parsed = rules.ParseState (channelId, meta, s);
    if (parsed == nullptr)
      return nullptr;

    auto* ptr = dynamic_cast<TestState*> (parsed.get ());
    CHECK (ptr != nullptr);

    parsed.release ();
    return std::unique_ptr<TestState> (ptr);
  }

};

TEST_F (ProtoBoardTests, ParseState)
{
  EXPECT_EQ (ParseState ("invalid"), nullptr);
  EXPECT_EQ (ParseState (TextState ("invalid: true")), nullptr);

  auto s = ParseState (TextState ("msg: \"foo\""));
  ASSERT_NE (s, nullptr);
  EXPECT_EQ (s->GetState ().msg (), "foo");
  EXPECT_EQ (s->GetChannelId (), channelId);
  EXPECT_EQ (&s->GetMetadata (), &meta);
}

TEST_F (ProtoBoardTests, Equals)
{
  auto p = ParseState (TextState ("msg: \"foo\""));
  EXPECT_TRUE (p->Equals (TextState ("msg: \"foo\"")));
  EXPECT_FALSE (p->Equals (TextState ("msg: \"bar\"")));
  EXPECT_FALSE (p->Equals ("invalid"));

  p = ParseState (TextState (R"(
    msg: "foo"
    only_compare_this: "value"
  )"));
  EXPECT_TRUE (p->Equals (TextState (R"(
    msg: "bar"
    only_compare_this: "value"
  )")));
  EXPECT_FALSE (p->Equals (TextState (R"(
    msg: "foo"
    only_compare_this: "other value"
  )")));
}

TEST_F (ProtoBoardTests, ApplyMove)
{
  auto p = ParseState (TextState ("msg: \"foo\""));

  BoardState newState;
  EXPECT_FALSE (p->ApplyMove (rpc, "invalid", newState));
  EXPECT_FALSE (p->ApplyMove (rpc, TextMove (""), newState));

  ASSERT_TRUE (p->ApplyMove (rpc, TextMove ("msg: \"bar\""), newState));
  proto::TestBoardState newPb;
  ASSERT_TRUE (newPb.ParseFromString (newState));
  EXPECT_EQ (newPb.msg (), "bar");
}

} // anonymous namespace
} // namespace xaya
