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
    : onChain(gameId, channelId, "player",
              mockXayaServer.GetClient (),
              mockXayaWallet.GetClient (),
              game.channel)
  {}

};

TEST_F (MoveSenderTests, SendMoveSuccess)
{
  const uint256 txid = SHA256::Hash ("txid");
  const std::string expectedValue = R"({"g":{"game id":[42,null,{"a":"b"}]}})";
  EXPECT_CALL (*mockXayaWallet, name_update ("p/player", expectedValue))
      .WillOnce (Return (txid.ToHex ()));

  EXPECT_EQ (onChain.SendMove (ParseJson (R"([
    42, null, {"a": "b"} 
  ])")), txid);
}

TEST_F (MoveSenderTests, SendMoveError)
{
  const std::string expectedValue = R"({"g":{"game id":{}}})";
  EXPECT_CALL (*mockXayaWallet, name_update ("p/player", expectedValue))
      .WillOnce (Throw (jsonrpc::JsonRpcException ("error")));

  EXPECT_TRUE (onChain.SendMove (ParseJson ("{}")).IsNull ());
}

TEST_F (MoveSenderTests, IsPending)
{
  const auto txidPending = SHA256::Hash ("txid 1");
  const auto txidOther = SHA256::Hash ("txid 2");
  const auto txidNotPending = SHA256::Hash ("txid 3");

  Json::Value pendings(Json::arrayValue);
  Json::Value p(Json::objectValue);
  p["name"] = "p/player";
  p["txid"] = txidPending.ToHex ();
  pendings.append (p);
  p = Json::Value (Json::objectValue);
  p["name"] = "p/other";
  p["txid"] = txidOther.ToHex ();
  pendings.append (p);

  EXPECT_CALL (*mockXayaServer, name_pending ())
      .WillRepeatedly (Return (pendings));

  EXPECT_TRUE (onChain.IsPending (txidPending));
  EXPECT_FALSE (onChain.IsPending (txidNotPending));
}

} // anonymous namespace
} // namespace xaya
