// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include "testutils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace xaya
{
namespace
{

using testing::Return;

class SignaturesTests : public testing::Test
{

protected:

  HttpRpcServer<MockXayaRpcServer> mockXayaServer;

};

TEST_F (SignaturesTests, InvalidSignature)
{
  EXPECT_CALL (*mockXayaServer,
               verifymessage ("", "my message", "my signature"))
      .WillOnce (Return (ParseJson (R"({"valid": false})")));
  EXPECT_EQ (VerifyMessage (mockXayaServer.GetClient (),
                            "my message", "my signature"),
             "invalid");
}

TEST_F (SignaturesTests, ValidSignature)
{
  EXPECT_CALL (*mockXayaServer,
               verifymessage ("", "my message", "my signature"))
      .WillOnce (Return (ParseJson (R"({"valid": true, "address": "addr"})")));
  EXPECT_EQ (VerifyMessage (mockXayaServer.GetClient (),
                            "my message", "my signature"),
             "addr");
}

} // anonymous namespace
} // namespace xaya
