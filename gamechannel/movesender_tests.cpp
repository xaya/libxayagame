// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "movesender.hpp"

#include "testgame.hpp"

#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using testing::Return;
using testing::Throw;

class MoveSenderTests : public TestGameFixture
{

protected:

  const std::string gameId = "game id";
  const uint256 channelId = SHA256::Hash ("channel id");

  MoveSender onChain;

  MoveSenderTests ()
    : onChain(gameId, channelId, "player", rpcWallet, game.channel)
  {}

};

TEST_F (MoveSenderTests, SendMoveSuccess)
{
  const uint256 txid = SHA256::Hash ("txid");
  const std::string expectedValue = R"({"g":{"game id":[42,null,{"a":"b"}]}})";
  EXPECT_CALL (mockXayaWallet, name_update ("p/player", expectedValue))
      .WillOnce (Return (txid.ToHex ()));

  EXPECT_EQ (onChain.SendMove (ParseJson (R"([
    42, null, {"a": "b"} 
  ])")), txid);
}

TEST_F (MoveSenderTests, SendMoveError)
{
  const std::string expectedValue = R"({"g":{"game id":{}}})";
  EXPECT_CALL (mockXayaWallet, name_update ("p/player", expectedValue))
      .WillOnce (Throw (jsonrpc::JsonRpcException ("error")));

  EXPECT_TRUE (onChain.SendMove (ParseJson ("{}")).IsNull ());
}

} // anonymous namespace
} // namespace xaya
