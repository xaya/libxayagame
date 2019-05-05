// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include "testutils.hpp"

#include "rpc-stubs/xayarpcclient.h"

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace xaya
{
namespace
{

using testing::Return;

class SignaturesTests : public testing::Test
{

private:

  jsonrpc::HttpServer httpServer;
  jsonrpc::HttpClient httpClient;

protected:

  MockXayaRpcServer mockXayaServer;
  XayaRpcClient rpcClient;

  SignaturesTests ()
    : httpServer(MockXayaRpcServer::HTTP_PORT),
      httpClient(MockXayaRpcServer::HTTP_URL),
      mockXayaServer(httpServer),
      rpcClient(httpClient)
  {
    mockXayaServer.StartListening ();
  }

  ~SignaturesTests ()
  {
    mockXayaServer.StopListening ();
  }

};

TEST_F (SignaturesTests, InvalidSignature)
{
  EXPECT_CALL (mockXayaServer, verifymessage ("", "my message", "my signature"))
      .WillOnce (Return (ParseJson (R"({"valid": false})")));
  EXPECT_EQ (VerifyMessage (rpcClient, "my message", "my signature"),
             "invalid");
}

TEST_F (SignaturesTests, ValidSignature)
{
  EXPECT_CALL (mockXayaServer, verifymessage ("", "my message", "my signature"))
      .WillOnce (Return (ParseJson (R"({"valid": true, "address": "addr"})")));
  EXPECT_EQ (VerifyMessage (rpcClient, "my message", "my signature"),
             "addr");
}

} // anonymous namespace
} // namespace xaya
