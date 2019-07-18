// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "broadcast.hpp"

#include "channelmanager.hpp"
#include "channelmanager_tests.hpp"

#include <google/protobuf/text_format.h>

#include <gtest/gtest.h>

#include <glog/logging.h>

#include <chrono>
#include <condition_variable>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;

/** Timeout for the waiters in the test broadcast.  */
constexpr auto WAITER_TIMEOUT = std::chrono::milliseconds (50);

/**
 * Implementation of OffChainBroadcast that simply feeds sent messages back
 * to GetMessages using a condition variable.
 */
class FeedBackBroadcast : public OffChainBroadcast
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

  explicit FeedBackBroadcast (ChannelManager& cm)
    : OffChainBroadcast(cm)
  {}

  using OffChainBroadcast::GetParticipants;

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

class BroadcastTests : public ChannelManagerTestFixture
{

protected:

  FeedBackBroadcast offChain;

  BroadcastTests ()
    : offChain(cm)
  {
    cm.SetOffChainBroadcast (offChain);
    offChain.Start ();
  }

  ~BroadcastTests ()
  {
    offChain.Stop ();
  }

  /**
   * Expects the given list of participants.
   */
  void
  ExpectParticipants (const std::set<std::string>& expected)
  {
    EXPECT_EQ (offChain.GetParticipants (), expected);
  }

};

TEST_F (BroadcastTests, Participants)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  ExpectParticipants ({"player", "other"});

  ProcessOnChainNonExistant ();
  ExpectParticipants ({});
}

TEST_F (BroadcastTests, FeedingMoves)
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

TEST_F (BroadcastTests, BeyondTimeout)
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
