// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using testing::Return;

class SignaturesTests : public TestGameFixture
{

protected:

  const uint256 channelId = SHA256::Hash ("channel id");

};

TEST_F (SignaturesTests, GetChannelSignatureMessage)
{
  const std::string dataWithNul("foo\0bar", 7);

  SHA256 hasher;
  hasher << channelId;
  hasher << std::string ("topic\0", 6);
  hasher << dataWithNul;

  const std::string actual = GetChannelSignatureMessage (channelId, "topic",
                                                         dataWithNul);
  EXPECT_EQ (actual, hasher.Finalise ().ToHex ());
  LOG (INFO) << "Signature message: " << actual;
}

TEST_F (SignaturesTests, InvalidTopic)
{
  const std::string invalidTopic("a\0b", 3);
  ASSERT_EQ (invalidTopic.size (), 3);
  ASSERT_EQ (invalidTopic[1], '\0');

  EXPECT_DEATH (GetChannelSignatureMessage (channelId, invalidTopic, "foobar"),
                "Topic string contains nul character");
}

TEST_F (SignaturesTests, VerifyParticipantSignatures)
{
  proto::ChannelMetadata meta;
  meta.add_participants ()->set_address ("address 1");
  meta.add_participants ()->set_address ("address 2");

  proto::SignedData data;
  data.set_data ("foobar");
  data.add_signatures ("signature 1");
  data.add_signatures ("signature 2");
  data.add_signatures ("signature 3");

  const std::string msg = GetChannelSignatureMessage (channelId, "topic",
                                                      data.data ());

  EXPECT_CALL (mockXayaServer,
               verifymessage ("", msg, EncodeBase64 ("signature 1")))
      .WillOnce (Return (ParseJson (R"({
        "valid": true,
        "address": "some other address"
      })")));
  EXPECT_CALL (mockXayaServer,
               verifymessage ("", msg, EncodeBase64 ("signature 2")))
      .WillOnce (Return (ParseJson (R"({
        "valid": true,
        "address": "address 2"
      })")));
  EXPECT_CALL (mockXayaServer,
               verifymessage ("", msg, EncodeBase64 ("signature 3")))
      .WillOnce (Return (ParseJson (R"({
        "valid": false
      })")));

  EXPECT_EQ (VerifyParticipantSignatures (rpcClient, channelId, meta,
                                          "topic", data),
             std::set<int> ({1}));
}

} // anonymous namespace
} // namespace xaya
