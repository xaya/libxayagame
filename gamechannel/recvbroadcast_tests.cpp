// Copyright (C) 2019-2026 The Xaya developers
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
#include <stdexcept>

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

  /** Number of messages that have been processed successfully.  */
  unsigned processed = 0;

  /** If set, the next GetMessages call will throw an exception.  */
  bool failNextGetMessages = false;

  /** If set, the next FeedMessage call will throw an exception.  */
  bool failNextFeedMessage = false;

  /** Minimum number of messages to wait for in GetMessages.  */
  size_t minBatch = 1;

protected:

  void
  SendMessage (const std::string& msg) override
  {
    std::lock_guard<std::mutex> lock(mut);
    messages.push_back (msg);
  }

  void
  FeedMessage (const std::string& msg) override
  {
    {
      std::lock_guard<std::mutex> lock(mut);
      if (failNextFeedMessage)
        {
          failNextFeedMessage = false;
          throw std::runtime_error ("injected FeedMessage error");
        }
    }

    ReceivingOffChainBroadcast::FeedMessage (msg);

    std::lock_guard<std::mutex> lock(mut);
    ++processed;
  }

  std::vector<std::string>
  GetMessages () override
  {
    std::unique_lock<std::mutex> lock(mut);

    if (failNextGetMessages)
      {
        failNextGetMessages = false;
        throw std::runtime_error ("injected GetMessages error");
      }

    if (messages.size () < minBatch)
      cv.wait_for (lock, WAITER_TIMEOUT);

    minBatch = 1;
    return std::move (messages);
  }

public:

  explicit FeedBackBroadcast (SynchronisedChannelManager& cm)
    : ReceivingOffChainBroadcast(cm)
  {}

  /**
   * Makes the next GetMessages call throw an exception.
   */
  void
  FailNextGetMessages ()
  {
    std::lock_guard<std::mutex> lock(mut);
    failNextGetMessages = true;
  }

  /**
   * Makes the next FeedMessage call throw an exception.
   */
  void
  FailNextFeedMessage ()
  {
    std::lock_guard<std::mutex> lock(mut);
    failNextFeedMessage = true;
  }

  /**
   * Sets the minimum number of queued messages to collect in GetMessages.
   */
  void
  SetMinBatch (const size_t n)
  {
    std::lock_guard<std::mutex> lock(mut);
    minBatch = n;
  }

  /**
   * Returns the number of messages processed successfully so far.
   */
  unsigned
  GetProcessed ()
  {
    std::lock_guard<std::mutex> lock(mut);
    return processed;
  }

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
  offChain.Notify ();

  /* Wait until both messages have been processed.  We cannot assume that
     they stay queued until Notify, as GetMessages returns them immediately
     once they are available (instead of waiting for the notification).
     The second message is for a reinit that is not known on-chain yet, so
     it gets queued internally and the visible state advances only to the
     first one.  */
  while (offChain.GetProcessed () < 2)
    SleepSome ();

  EXPECT_EQ (offChain.GetProcessed (), 2);
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

TEST_F (ReceivingBroadcastTests, FeedExceptionsRetryBatch)
{
  ProcessOnChain ("0 0", ValidProof ("0 0"), 0);

  /* Send two messages as a single batch.  Processing the first one fails
     once, but the loop must retry it and then process the second one as well,
     rather than dropping the whole batch.  */
  offChain.SetMinBatch (2);
  offChain.FailNextFeedMessage ();
  offChain.SendNewState ("", ValidProof ("10 5"));
  offChain.SendNewState ("", ValidProof ("20 6"));
  offChain.Notify ();

  for (unsigned i = 0; i < 500 && offChain.GetProcessed () < 2; ++i)
    SleepSome ();

  EXPECT_EQ (offChain.GetProcessed (), 2);
  EXPECT_EQ (GetLatestState (), "20 6");
}

TEST_F (ReceivingBroadcastTests, ExceptionsAreRetried)
{
  ProcessOnChain ("0 0", ValidProof ("0 0"), 0);

  offChain.FailNextGetMessages ();
  offChain.Notify ();
  SleepSome ();

  offChain.SendNewState ("", ValidProof ("10 5"));
  offChain.Notify ();

  /* The loop should survive the exception, wait for its retry delay and
     then process the queued update.  */
  while (true)
    {
      SleepSome ();
      const BoardState state = GetLatestState ();
      if (state == "0 0")
        continue;

      EXPECT_EQ (GetLatestState (), "10 5");
      break;
    }
}

} // anonymous namespace
} // namespace xaya
