// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "recvbroadcast.hpp"

#include "channelmanager.hpp"
#include "channelmanager_tests.hpp"

#include <gtest/gtest.h>

#include <glog/logging.h>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace xaya
{
namespace
{

/** Timeout for the waiters in the test broadcast.  */
constexpr auto WAITER_TIMEOUT = std::chrono::milliseconds (50);

/**
 * Implementation of OffChainBroadcast that simply feeds sent messages back
 * to GetMessages using a condition variable.
 */
class FeedBackBroadcast : public ReceivingOffChainBroadcast
{

private:

  std::mutex mut;
  std::condition_variable cv;

  /** Current list of unforwarded messages.  */
  std::vector<std::string> messages;

protected:

  void
  SendMessage (const std::string& msg) override
  {
    std::lock_guard<std::mutex> lock(mut);
    messages.push_back (msg);
  }

  std::vector<std::string>
  GetMessages () override
  {
    std::unique_lock<std::mutex> lock(mut);
    if (messages.empty ())
      cv.wait_for (lock, WAITER_TIMEOUT);

    return std::move (messages);
  }

public:

  explicit FeedBackBroadcast (SynchronisedChannelManager& cm)
    : ReceivingOffChainBroadcast(cm)
  {}

  /**
   * Forwards the queued messages (by notifying the waiting thread).
   */
  void
  Notify ()
  {
    std::lock_guard<std::mutex> lock(mut);
    cv.notify_all ();
  }

};

class ReceivingBroadcastTests : public ChannelManagerTestFixture
{

protected:

  /**
   * SynchronisedChannelManager based on the fixture's manager.  We need that
   * so we can instantiate the ReceivingOffChainBroadcast.  Otherwise we do
   * not use the lock here, as the offchain broadcast's loop doesn't actually
   * access the channel manager in any way.
   */
  SynchronisedChannelManager scm;

  FeedBackBroadcast offChain;

  ReceivingBroadcastTests ()
    : scm(cm), offChain(scm)
  {
    cm.SetOffChainBroadcast (offChain);
    offChain.Start ();
  }

  ~ReceivingBroadcastTests ()
  {
    offChain.Stop ();
  }

};

TEST_F (ReceivingBroadcastTests, FeedingMoves)
{
  meta.set_reinit ("reinit");
  ProcessOnChain ("0 0", ValidProof ("1 2"), 0);

  meta.clear_reinit ();
  ProcessOnChain ("0 0", ValidProof ("0 0"), 0);

  offChain.SendNewState ("", ValidProof ("10 5"));
  offChain.SendNewState ("reinit", ValidProof ("9 10"));

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "0 0");

  offChain.Notify ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");

  meta.set_reinit ("reinit");
  ProcessOnChain ("0 0", ValidProof ("1 2"), 0);
  EXPECT_EQ (GetLatestState (), "9 10");
}

TEST_F (ReceivingBroadcastTests, BeyondTimeout)
{
  ProcessOnChain ("0 0", ValidProof ("0 0"), 0);
  offChain.SendNewState ("", ValidProof ("10 5"));

  /* Even without a notification, we will get the new state because the
     waiter thread times out and recalls GetMessages.  */
  std::this_thread::sleep_for (2 * WAITER_TIMEOUT);
  EXPECT_EQ (GetLatestState (), "10 5");
}

} // anonymous namespace
} // namespace xaya
