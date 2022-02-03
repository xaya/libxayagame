// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "syncmanager.hpp"

#include "channelmanager_tests.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <thread>

namespace xaya
{
namespace
{

using testing::_;

class WaitForChangeTests : public ChannelManagerTestFixture
{

private:

  /** The thread that is used to call WaitForChange.  */
  std::unique_ptr<std::thread> waiter;

  /** Set to true while the thread is actively waiting.  */
  bool waiting;

  /** Lock for waiting.  */
  mutable std::mutex mut;

  /** The JSON value returned from WaitForChange.  */
  Json::Value returnedJson;

protected:

  /** Our synchronised manager for waiting.  */
  SynchronisedChannelManager scm;

  MockOffChainBroadcast offChain;

  WaitForChangeTests ()
    : scm(cm), offChain(cm.GetChannelId ())
  {
    cm.SetOffChainBroadcast (offChain);
  }

  /**
   * Calls WaitForChange on a newly started thread.
   */
  void
  CallWaitForChange (
      int known = SynchronisedChannelManager::WAITFORCHANGE_ALWAYS_BLOCK)
  {
    CHECK (waiter == nullptr);
    waiter = std::make_unique<std::thread> ([this, known] ()
      {
        LOG (INFO) << "Calling WaitForChange...";
        {
          std::lock_guard<std::mutex> lock(mut);
          waiting = true;
        }
        returnedJson = scm.WaitForChange (known);
        {
          std::lock_guard<std::mutex> lock(mut);
          waiting = false;
        }
        LOG (INFO) << "WaitForChange returned";
      });

    /* Make sure the thread had time to start and make the call.  */
    SleepSome ();
  }

  /**
   * Waits for the waiter thread to return and checks that the JSON value
   * from it matches the then-correct ToJson output.  Also expects that the
   * thread is finished "soon" (rather than timeout later).
   */
  void
  JoinWaiter ()
  {
    CHECK (waiter != nullptr);

    SleepSome ();
    EXPECT_FALSE (IsWaiting ());

    LOG (INFO) << "Joining the waiter thread...";
    waiter->join ();
    LOG (INFO) << "Waiter thread finished";
    waiter.reset ();
    ASSERT_EQ (returnedJson, cm.ToJson ());
  }

  /**
   * Returns true if the thread is currently waiting.
   */
  bool
  IsWaiting () const
  {
    CHECK (waiter != nullptr);

    std::lock_guard<std::mutex> lock(mut);
    return waiting;
  }

};

TEST_F (WaitForChangeTests, OnChain)
{
  CallWaitForChange ();
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, OnChainNonExistant)
{
  CallWaitForChange ();
  ProcessOnChainNonExistant ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, OffChain)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);

  CallWaitForChange ();
  cm.ProcessOffChain ("", ValidProof ("12 6"));
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, OffChainNoChange)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);

  CallWaitForChange ();
  cm.ProcessOffChain ("", ValidProof ("10 5"));

  SleepSome ();
  EXPECT_TRUE (IsWaiting ());

  scm.StopUpdates ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, LocalMove)
{
  /* We don't care about the broadcast, just specify that one will
     be triggered.  */
  EXPECT_CALL (offChain, SendMessage (_)).Times (1);

  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);

  CallWaitForChange ();
  cm.ProcessLocalMove ("1");
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, WhenStopped)
{
  scm.StopUpdates ();
  CallWaitForChange ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, StopNotifies)
{
  CallWaitForChange ();
  scm.StopUpdates ();
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, OutdatedKnownVersion)
{
  const int known = cm.ToJson ()["version"].asInt ();
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  CallWaitForChange (known);
  JoinWaiter ();
}

TEST_F (WaitForChangeTests, UpToDateKnownVersion)
{
  const int known = cm.ToJson ()["version"].asInt ();
  CallWaitForChange (known);

  SleepSome ();
  EXPECT_TRUE (IsWaiting ());

  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  JoinWaiter ();
}

} // anonymous namespace
} // namespace xaya
