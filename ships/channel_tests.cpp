// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channel.hpp"

#include "board.hpp"
#include "proto/boardstate.pb.h"
#include "proto/winnerstatement.pb.h"
#include "testutils.hpp"

#include <gamechannel/protoutils.hpp>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayagame/testutils.hpp>
#include <xayautil/hash.hpp>

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace ships
{
namespace
{

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;
using testing::Return;
using testing::Truly;

/**
 * Parses a text-format state proto.
 */
proto::BoardState
TextState (const std::string& str)
{
  proto::BoardState res;
  CHECK (TextFormat::ParseFromString (str, &res));
  return res;
}

/**
 * Parses a text-format state proof proto.
 */
xaya::proto::StateProof
TextProof (const std::string& str)
{
  xaya::proto::StateProof res;
  CHECK (TextFormat::ParseFromString (str, &res));
  return res;
}

/**
 * Builds a winner statement with (fake) signatures for the given winner.
 */
xaya::proto::SignedData
FakeWinnerStatement (const int winner)
{
  proto::WinnerStatement stmt;
  stmt.set_winner (winner);

  xaya::proto::SignedData res;
  CHECK (stmt.SerializeToString (res.mutable_data ()));
  res.add_signatures ("sgn");

  return res;
}

class ChannelTests : public testing::Test
{

protected:

  const xaya::uint256 channelId = xaya::SHA256::Hash ("foo");

  /** In the metadata, "we" are player 0.  */
  xaya::proto::ChannelMetadata meta;

  ShipsBoardRules rules;
  ShipsChannel channel;

  ChannelTests ()
    : channel("player")
  {
    CHECK (TextFormat::ParseFromString (R"(
      participants:
        {
          name: "player"
          address: "my addr"
        }
      participants:
        {
          name: "other player"
          address: "other addr"
        }
    )", &meta));
  }

  /**
   * Parses a BoardState proto into a ParsedBoardState.
   */
  std::unique_ptr<xaya::ParsedBoardState>
  ParseState (const proto::BoardState& pb)
  {
    std::string serialised;
    CHECK (pb.SerializeToString (&serialised));

    auto res = rules.ParseState (channelId, meta, serialised);
    CHECK (res != nullptr);

    return res;
  }

};

/* ************************************************************************** */

class OnChainMoveTests : public ChannelTests
{

private:

  jsonrpc::HttpServer httpServerWallet;
  jsonrpc::HttpClient httpClientWallet;

protected:

  xaya::MockXayaWalletRpcServer mockXayaWallet;
  XayaWalletRpcClient rpcWallet;

  xaya::MoveSender sender;

  OnChainMoveTests ()
    : httpServerWallet(xaya::MockXayaWalletRpcServer::HTTP_PORT),
      httpClientWallet(xaya::MockXayaWalletRpcServer::HTTP_URL),
      mockXayaWallet(httpServerWallet),
      rpcWallet(httpClientWallet),
      sender("xs", channelId, "player", rpcWallet, channel)
  {
    mockXayaWallet.StartListening ();
  }

  ~OnChainMoveTests ()
  {
    mockXayaWallet.StopListening ();
  }

  /**
   * Verifies that a given JSON object matches the expected move format
   * for the given key ("r", "d" or "w"), channel ID and encoded data proto.
   * Note that all three types of moves (disputes, resolutions and channel
   * closes with WinnerStatement's) have the same basic structure.
   */
  template <typename Proto>
    static bool
    IsExpectedMove (const Json::Value& actual,
                    const std::string& key, const std::string& protoKey,
                    const xaya::uint256& id, const Proto& expectedPb)
  {
    if (!actual.isObject ())
      return false;
    if (actual.size () != 1)
      return false;

    const auto& sub = actual[key];
    if (!sub.isObject ())
      return false;
    if (sub.size () != 2)
      return false;

    if (!sub["id"].isString ())
      return false;
    if (sub["id"].asString () != id.ToHex ())
      return false;

    if (!sub[protoKey].isString ())
      return false;

    Proto actualPb;
    if (!xaya::ProtoFromBase64 (sub[protoKey].asString (), actualPb))
      return false;

    return MessageDifferencer::Equals (actualPb, expectedPb);
  }

};

TEST_F (OnChainMoveTests, ResolutionMove)
{
  const auto proof = TextProof (R"(
    initial_state:
      {
        data: ""
        signatures: "sgn 0"
      }
  )");

  EXPECT_TRUE (IsExpectedMove (channel.ResolutionMove (channelId, proof),
                               "r", "state", channelId, proof));
}

TEST_F (OnChainMoveTests, DisputeMove)
{
  const auto proof = TextProof (R"(
    initial_state:
      {
        data: ""
        signatures: "sgn 0"
      }
  )");

  EXPECT_TRUE (IsExpectedMove (channel.DisputeMove (channelId, proof),
                               "d", "state", channelId, proof));
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveNotFinished)
{
  channel.MaybeOnChainMove (*ParseState (TextState ("turn: 0")), sender);
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveNotMe)
{
  proto::BoardState state;
  *state.mutable_winner_statement () = FakeWinnerStatement (1);

  channel.MaybeOnChainMove (*ParseState (state), sender);
}

TEST_F (OnChainMoveTests, MaybeOnChainMoveSending)
{
  const auto stmt = FakeWinnerStatement (0);

  const auto isOk = [this, stmt] (const std::string& str)
    {
      const auto val = ParseJson (str);
      const auto& mv = val["g"]["xs"];
      return IsExpectedMove (mv, "w", "stmt", channelId, stmt);
    };
  EXPECT_CALL (mockXayaWallet, name_update ("p/player", Truly (isOk)))
      .WillOnce (Return (xaya::SHA256::Hash ("txid").ToHex ()));

  proto::BoardState state;
  *state.mutable_winner_statement () = stmt;

  channel.MaybeOnChainMove (*ParseState (state), sender);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
