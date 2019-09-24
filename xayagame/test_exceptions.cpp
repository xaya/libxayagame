// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc-stubs/testrpcclient.h"
#include "rpc-stubs/testrpcserverstub.h"

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <glog/logging.h>

#include <stdexcept>
#include <iostream>

namespace
{

/**
 * Tests simple throw of an exception without further ado.
 */
void
BasicThrow ()
{
  LOG (INFO) << "Testing basic throw and catch...";

  try
    {
      LOG (INFO) << "Throwing exception...";
      throw std::runtime_error ("test exception");
      LOG (FATAL) << "Exception not thrown!";
    }
  catch (const std::exception& exc)
    {
      LOG (INFO) << "Caught exception: " << exc.what ();
    }
}

/**
 * RPC server that implements the "test" interface and just throws an exception.
 */
class TestRpcServer : public TestRpcServerStub
{

public:

  using TestRpcServerStub::TestRpcServerStub;

  int
  add (const int a, const int b) override
  {
    throw jsonrpc::JsonRpcException (a + b, "expected error");
  }

};

/**
 * Tests an exception thrown through libjson-rpc-cpp.
 */
void
JsonRpcThrow ()
{
  LOG (INFO) << "Testing exceptions in JSON-RPC...";

  jsonrpc::HttpServer httpServer(12345);
  TestRpcServer rpcServer(httpServer);
  rpcServer.StartListening ();

  jsonrpc::HttpClient httpClient("http://localhost:12345");
  TestRpcClient rpcClient(httpClient);

  try
    {
      LOG (INFO) << "Making call that will throw...";
      rpcClient.add (1, 2);
      LOG (FATAL) << "Exception not thrown!";
    }
  catch (const jsonrpc::JsonRpcException& exc)
    {
      LOG (INFO) << "Caught exception: " << exc.what ();
      CHECK_EQ (exc.GetCode (), 3);
    }
}

} // anonymous namespace

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);

  BasicThrow ();
  JsonRpcThrow ();

  return EXIT_SUCCESS;
}
