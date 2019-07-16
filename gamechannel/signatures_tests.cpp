// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using testing::_;
using testing::Return;
using testing::Throw;

class SignaturesTests : public TestGameFixture
{

protected:

  const uint256 channelId = SHA256::Hash ("channel id");
  proto::ChannelMetadata meta;

  SignaturesTests ()
  {
    meta.set_reinit (std::string ("re\0init", 7));
    meta.add_participants ()->set_address ("address 0");
    meta.add_participants ()->set_address ("address 1");

    ValidSignature ("sgn 0", "address 0");
    ValidSignature ("sgn 1", "address 1");
  }

};

TEST_F (SignaturesTests, GetChannelSignatureMessage)
{
  const std::string dataWithNul("foo\0bar", 7);

  SHA256 hasher;
  hasher << channelId;
  hasher << EncodeBase64 (std::string ("re\0init", 7))
         << std::string ("\0", 1);
  hasher << std::string ("topic\0", 6);
  hasher << dataWithNul;

  const std::string actual
      = GetChannelSignatureMessage (channelId, meta, "topic", dataWithNul);
  EXPECT_EQ (actual, hasher.Finalise ().ToHex ());
  LOG (INFO) << "Signature message: " << actual;
}

TEST_F (SignaturesTests, InvalidTopic)
{
  const std::string invalidTopic("a\0b", 3);
  ASSERT_EQ (invalidTopic.size (), 3);
  ASSERT_EQ (invalidTopic[1], '\0');

  EXPECT_DEATH (GetChannelSignatureMessage (channelId, meta,
                                            invalidTopic, "foobar"),
                "Topic string contains nul character");
}

TEST_F (SignaturesTests, VerifyParticipantSignatures)
{
  proto::SignedData data;
  data.set_data ("foobar");
  data.add_signatures ("signature 0");
  data.add_signatures ("signature 1");
  data.add_signatures ("signature 2");

  const std::string msg = GetChannelSignatureMessage (channelId, meta, "topic",
                                                      data.data ());

  EXPECT_CALL (mockXayaServer,
               verifymessage ("", msg, EncodeBase64 ("signature 0")))
      .WillOnce (Return (ParseJson (R"({
        "valid": true,
        "address": "some other address"
      })")));
  ExpectSignature (channelId, meta, "topic", data.data (),
                   "signature 1", "address 1");
  EXPECT_CALL (mockXayaServer,
               verifymessage ("", msg, EncodeBase64 ("signature 2")))
      .WillOnce (Return (ParseJson (R"({
        "valid": false
      })")));

  EXPECT_EQ (VerifyParticipantSignatures (rpcClient, channelId, meta,
                                          "topic", data),
             std::set<int> ({1}));
}

TEST_F (SignaturesTests, SignDataForParticipantError)
{
  EXPECT_CALL (mockXayaWallet, signmessage ("address 1", _))
      .WillOnce (Throw (jsonrpc::JsonRpcException (-5)));

  proto::SignedData data;
  data.set_data ("foobar");
  data.add_signatures ("sgn 0");

  EXPECT_FALSE (SignDataForParticipant (rpcWallet, channelId, meta, "topic",
                                        1, data));

  EXPECT_EQ (VerifyParticipantSignatures (rpcClient, channelId, meta,
                                          "topic", data),
             std::set<int> ({0}));
}

TEST_F (SignaturesTests, SignDataForParticipantSuccess)
{
  proto::SignedData data;
  data.set_data ("foobar");
  data.add_signatures ("sgn 0");

  const std::string msg = GetChannelSignatureMessage (channelId, meta, "topic",
                                                      data.data ());
  EXPECT_CALL (mockXayaWallet, signmessage ("address 1", msg))
      .WillOnce (Return (EncodeBase64 ("signature 1")));
  ExpectSignature (channelId, meta, "topic", data.data (),
                   "signature 1", "address 1");

  ASSERT_TRUE (SignDataForParticipant (rpcWallet, channelId, meta, "topic",
                                       1, data));

  EXPECT_EQ (VerifyParticipantSignatures (rpcClient, channelId, meta,
                                          "topic", data),
             std::set<int> ({0, 1}));
}

} // anonymous namespace
} // namespace xaya
