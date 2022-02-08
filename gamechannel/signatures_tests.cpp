// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <sstream>

namespace xaya
{
namespace
{

using testing::_;
using testing::Return;

class SignaturesTests : public TestGameFixture
{

protected:

  const std::string gameId = "game id";
  const uint256 channelId = SHA256::Hash ("channel id");
  proto::ChannelMetadata meta;

  SignaturesTests ()
  {
    meta.set_reinit (std::string ("re\0init", 7));
    meta.add_participants ()->set_address ("address 0");
    meta.add_participants ()->set_address ("address 1");

    verifier.SetValid ("sgn 0", "address 0");
    verifier.SetValid ("sgn 1", "address 1");
  }

};

TEST_F (SignaturesTests, GetChannelSignatureMessage)
{
  const std::string dataWithNul("foo\0bar", 7);

  std::stringstream expected;
  expected << "Game-Channel Signature\n"
           << "Game ID: " << gameId << "\n"
           << "Channel: " << channelId.ToHex () << "\n"
           << "Reinit: " << EncodeBase64 (std::string ("re\0init", 7)) << "\n"
           << "Topic: topic\n"
           << "Data Hash: " << SHA256::Hash (dataWithNul).ToHex ();

  const std::string actual
      = GetChannelSignatureMessage (gameId, channelId, meta,
                                    "topic", dataWithNul);
  EXPECT_EQ (actual, expected.str ());
  LOG (INFO) << "Signature message:\n" << actual;
}

TEST_F (SignaturesTests, InvalidTopic)
{
  const std::string invalidTopic("a\nb");
  EXPECT_DEATH (GetChannelSignatureMessage (gameId, channelId, meta,
                                            invalidTopic, "foobar"),
                "Topic string contains invalid character");
}

TEST_F (SignaturesTests, VerifyParticipantSignatures)
{
  proto::SignedData data;
  data.set_data ("foobar");
  data.add_signatures ("signature 0");
  data.add_signatures ("signature 1");
  data.add_signatures ("signature 2");

  verifier.ExpectOne (gameId, channelId, meta, "topic", data.data (),
                      "signature 0", "some other address");
  verifier.ExpectOne (gameId, channelId, meta, "topic", data.data (),
                      "signature 1", "address 1");
  verifier.ExpectOne (gameId, channelId, meta, "topic", data.data (),
                      "signature 2", "invalid");

  EXPECT_EQ (VerifyParticipantSignatures (verifier, gameId, channelId, meta,
                                          "topic", data),
             std::set<int> ({1}));
}

TEST_F (SignaturesTests, SignDataForParticipantError)
{
  proto::SignedData data;
  data.set_data ("foobar");
  data.add_signatures ("sgn 0");

  signer.SetAddress ("wrong address");
  EXPECT_FALSE (SignDataForParticipant (signer, gameId, channelId, meta,
                                        "topic", 1, data));

  EXPECT_EQ (VerifyParticipantSignatures (verifier, gameId, channelId, meta,
                                          "topic", data),
             std::set<int> ({0}));
}

TEST_F (SignaturesTests, SignDataForParticipantSuccess)
{
  proto::SignedData data;
  data.set_data ("foobar");
  data.add_signatures ("sgn 0");

  const std::string msg
      = GetChannelSignatureMessage (gameId, channelId, meta,
                                    "topic", data.data ());
  signer.SetAddress ("address 1");
  EXPECT_CALL (signer, SignMessage (msg)).WillOnce (Return ("signature 1"));
  verifier.ExpectOne (gameId, channelId, meta, "topic", data.data (),
                      "signature 1", "address 1");

  ASSERT_TRUE (SignDataForParticipant (signer, gameId, channelId, meta,
                                       "topic", 1, data));

  EXPECT_EQ (VerifyParticipantSignatures (verifier, gameId, channelId, meta,
                                          "topic", data),
             std::set<int> ({0, 1}));
}

} // anonymous namespace
} // namespace xaya
