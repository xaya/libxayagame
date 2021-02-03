// Copyright (C) 2018-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "rpcbroadcast.hpp"

#include <xayautil/hash.hpp>
#include <xayautil/uint256.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <google/protobuf/stubs/common.h>

#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace
{

DEFINE_string (rpc_url, "",
               "URL at which the broadcast server's RPC interface is");

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
 * Broadcast channel based on the RpcBroadcast, but without a ChannelManager
 * and recording the received messages.  The main test uses multiple
 * of them connected to the same server to send messages and test receiving
 * of them as expected.
 */
class TestRpcBroadcast : public RpcBroadcast
{

private:

  /** Lock for the received messages.  */
  std::mutex mut;

  /** Condition variable signalled when a new message is received.  */
  std::condition_variable cv;

  /** The received and not yet "expected" messages.  */
  std::vector<std::string> messages;

protected:

  void
  FeedMessage (const std::string& msg) override
  {
    std::lock_guard<std::mutex> lock(mut);
    messages.push_back (msg);
    cv.notify_one ();
  }

public:

  explicit TestRpcBroadcast (const std::string& rpcUrl, const uint256& id)
    : RpcBroadcast(rpcUrl, id)
  {
    Start ();
  }

  ~TestRpcBroadcast ()
  {
    Stop ();
    CHECK (messages.empty ()) << "Unexpected messages: " << messages;
  }

  /**
   * Expects that the received messages match the given list.  Waits for
   * more if necessary.
   */
  void
  ExpectResult (const std::vector<std::string>& expected)
  {
    std::unique_lock<std::mutex> lock(mut);
    while (messages.size () < expected.size ())
      cv.wait (lock);

    if (messages != expected)
      {
        LOG (FATAL)
            << "Messages do not match expectations!"
               "\nActual: " << messages
            << "\nExpected: " << expected;
      }
    messages.clear ();
  }

  using RpcBroadcast::SendMessage;

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
  bc1.SendMessage ("foo");
  bc1.ExpectResult ({"foo"});

  TestRpcBroadcast bc2(FLAGS_rpc_url, id2);
  bc2.SendMessage ("bar");
  bc2.ExpectResult ({"bar"});

  bc1.SendMessage ("baz");
  TestRpcBroadcast bc3(FLAGS_rpc_url, id1);
  bc3.SendMessage ("abc");
  bc1.ExpectResult ({"baz", "abc"});
  bc3.ExpectResult ({"abc"});

  /* Test a string that is not valid UTF-8.  */
  const std::string weirdStr("abc\0def\xFF", 8);
  bc2.SendMessage (weirdStr);
  bc2.ExpectResult ({weirdStr});

  google::protobuf::ShutdownProtobufLibrary ();
  return EXIT_SUCCESS;
}
