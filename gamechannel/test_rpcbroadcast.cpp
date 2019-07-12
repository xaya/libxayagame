// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "channelmanager.hpp"
#include "rpcbroadcast.hpp"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayautil/hash.hpp>
#include <xayautil/uint256.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <google/protobuf/stubs/common.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

DEFINE_string (rpc_url, "",
               "URL at which the broadcast server's RPC interface is");

/**
 * Returns a fake reference to an instance of the given type.  The value
 * will be a nullptr.  This is used to quickly stub out dependencies for
 * ChannelManager that are not actually accessed in our tests here.
 */
template <typename T>
  T&
  NullReference ()
{
  return *static_cast<T*> (nullptr);
}

} // anonymous namespace

/**
 * Utility method to print vectors of messages for CHECK_EQ.
 */
template <typename S>
  S&
  operator<< (S& out, const std::vector<std::string>& msg)
{
  out << "[";
  for (const auto& m : msg)
    out << " " << m;
  out << " ]";

  return out;
}

namespace xaya
{

/**
 * Broadcast channel with faked ChannelManager.  The main test uses multiple
 * of them connected to the same server to send messages and test receiving
 * of them as expected.
 */
class TestRpcBroadcast
{

private:

  /** The faked ChannelManager.  */
  ChannelManager cm;

  /** The broadcast channel itself.  */
  RpcBroadcast bc;

  /** Thread running a pending call to GetMessages.  */
  std::unique_ptr<std::thread> runningCall;

  /** The result of the last GetMessages call.  */
  std::vector<std::string> messages;

public:

  explicit TestRpcBroadcast (const std::string& rpcUrl, const uint256& id)
    : cm(NullReference<BoardRules> (), NullReference<XayaRpcClient> (),
         NullReference<XayaWalletRpcClient> (), id, "player name"),
      bc(rpcUrl, cm)
  {
    /* We do not want to run the broadcast's event loop (as that would mess
       up our custom calls to SendMessage and GetMessages).  We still need
       to initialise the sequence number, though, just like Start() would
       do in a real usage situation.  */
    bc.InitialiseSequence ();
  }

  ~TestRpcBroadcast ()
  {
    cm.StopUpdates ();
  }

  /**
   * Sends a message on the channel.
   */
  void
  Send (const std::string& msg)
  {
    bc.SendMessage (msg);
  }

  /**
   * Starts a new call to GetMessages in a new thread.
   */
  void
  StartReceive ()
  {
    CHECK (runningCall == nullptr);
    runningCall = std::make_unique<std::thread> ([this] ()
      {
        messages = bc.GetMessages ();
      });
  }

  /**
   * Waits for the last call to GetMessages to finish and checks that
   * the received messages match the expectations.
   */
  void
  ExpectResult (const std::vector<std::string>& expected)
  {
    CHECK (runningCall != nullptr);
    runningCall->join ();
    runningCall.reset ();
    if (messages != expected)
      {
        LOG (FATAL)
            << "Messages do not match expectations!"
               "\nActual: " << messages
            << "\nExpected: " << expected;
      }
  }

};

} // namespace xaya

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  gflags::SetUsageMessage ("Run RPC broadcast tests");
  gflags::SetVersionString (PACKAGE_VERSION);
  gflags::ParseCommandLineFlags (&argc, &argv, true);

  if (FLAGS_rpc_url.empty ())
    {
      std::cerr << "Error: --rpc_url must be set" << std::endl;
      return EXIT_FAILURE;
    }

  const auto id1 = xaya::SHA256::Hash ("channel 1");
  const auto id2 = xaya::SHA256::Hash ("channel 2");

  using xaya::TestRpcBroadcast;

  TestRpcBroadcast bc1(FLAGS_rpc_url, id1);
  bc1.Send ("foo");
  bc1.StartReceive ();
  bc1.ExpectResult ({"foo"});

  TestRpcBroadcast bc2(FLAGS_rpc_url, id2);
  bc2.StartReceive ();
  bc2.Send ("bar");
  bc2.ExpectResult ({"bar"});

  bc1.Send ("baz");
  TestRpcBroadcast bc3(FLAGS_rpc_url, id1);
  bc3.Send ("abc");
  bc1.StartReceive ();
  bc1.ExpectResult ({"baz", "abc"});
  bc3.StartReceive ();
  bc3.ExpectResult ({"abc"});

  /* Test a string that is not valid UTF-8.  */
  const std::string weirdStr("abc\0def\xFF", 8);
  bc2.Send (weirdStr);
  bc2.StartReceive ();
  bc2.ExpectResult ({weirdStr});

  google::protobuf::ShutdownProtobufLibrary ();
  return EXIT_SUCCESS;
}
