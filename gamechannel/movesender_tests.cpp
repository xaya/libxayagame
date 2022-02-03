// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "movesender.hpp"

#include "testgame.hpp"

#include <xayautil/hash.hpp>

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

  MockTransactionSender txSender;
  MoveSender sender;

  MoveSenderTests ()
    : sender(gameId, channelId, "player", txSender, game.channel)
  {}

};

TEST_F (MoveSenderTests, SendMoveSuccess)
{
  const std::string expectedValue = R"({"g":{"game id":[42,null,{"a":"b"}]}})";
  const uint256 txid = txSender.ExpectSuccess ("player", expectedValue);

  EXPECT_EQ (sender.SendMove (ParseJson (R"([
    42, null, {"a": "b"} 
  ])")), txid);
}

TEST_F (MoveSenderTests, SendMoveError)
{
  const std::string expectedValue = R"({"g":{"game id":{}}})";
  txSender.ExpectFailure ("player", expectedValue);

  EXPECT_TRUE (sender.SendMove (ParseJson ("{}")).IsNull ());
}

} // anonymous namespace
} // namespace xaya
