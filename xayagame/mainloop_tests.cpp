// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mainloop.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

namespace xaya
{
namespace internal
{

class MainLoopTests : public testing::Test
{

protected:

  /* Variables recording whether the start/stop functors were called.  */
  bool startCalled = false;
  bool stopCalled = false;

  /** Thread running the main loop.  */
  std::unique_ptr<std::thread> loopThread;

  /**
   * Constructs a functor that sets the given flag to true when called and
   * verifies that it was not already true.
   */
  static MainLoop::Functor
  FlagFunctor (bool& flag)
  {
    return [&flag] ()
      {
        EXPECT_FALSE (flag);
        flag = true;
      };
  }

  /**
   * Sleep for "some time" to give the main loop thread time to run and simulate
   * some delay in a real application.
   */
  static void
  SleepSome ()
  {
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }

  /**
   * Calls Run() on the given main loop in a separate thread, passing functors
   * that flip our startCalled/stopCalled flags.
   */
  void
  RunWithFlags (MainLoop& loop)
  {
    ASSERT_EQ (loopThread, nullptr);
    loopThread = std::make_unique<std::thread> ([this, &loop] ()
      {
        loop.Run (FlagFunctor (startCalled), FlagFunctor (stopCalled));
      });

    /* Wait until the start functor has been called.  */
    while (!startCalled)
      std::this_thread::yield ();
  }

  /**
   * Stops the main loop started in a separate thread and joins the thread.
   */
  void
  StopAndJoin (MainLoop& loop)
  {
    loop.Stop ();
    ASSERT_NE (loopThread, nullptr);
    loopThread->join ();
    loopThread.reset ();
  }

  /**
   * Exposes MainLoop::HandleInterrupt to the test.
   */
  static void
  HandleInterrupt (const int signum)
  {
    MainLoop::HandleInterrupt (signum);
  }

};

namespace
{

TEST_F (MainLoopTests, LoopWithStop)
{
  MainLoop loop;

  EXPECT_FALSE (startCalled);
  EXPECT_FALSE (stopCalled);
  EXPECT_FALSE (loop.IsRunning ());

  RunWithFlags (loop);
  EXPECT_TRUE (startCalled);
  EXPECT_FALSE (stopCalled);
  EXPECT_TRUE (loop.IsRunning ());

  SleepSome ();
  EXPECT_TRUE (startCalled);
  EXPECT_FALSE (stopCalled);
  EXPECT_TRUE (loop.IsRunning ());

  StopAndJoin (loop);
  EXPECT_TRUE (startCalled);
  EXPECT_TRUE (stopCalled);
  EXPECT_FALSE (loop.IsRunning ());
}

TEST_F (MainLoopTests, LoopWithInterrupt)
{
  MainLoop loop;

  EXPECT_FALSE (startCalled);
  EXPECT_FALSE (stopCalled);
  EXPECT_FALSE (loop.IsRunning ());

  RunWithFlags (loop);
  EXPECT_TRUE (startCalled);
  EXPECT_FALSE (stopCalled);
  EXPECT_TRUE (loop.IsRunning ());

  HandleInterrupt (SIGINT);
  ASSERT_NE (loopThread, nullptr);
  loopThread->join ();
  EXPECT_TRUE (startCalled);
  EXPECT_TRUE (stopCalled);
  EXPECT_FALSE (loop.IsRunning ());
}

TEST_F (MainLoopTests, CanRunMultipleTimes)
{
  MainLoop loop;

  for (int i = 0; i < 5; ++i)
    {
      EXPECT_FALSE (startCalled);
      EXPECT_FALSE (stopCalled);
      EXPECT_FALSE (loop.IsRunning ());

      RunWithFlags (loop);
      EXPECT_TRUE (startCalled);
      EXPECT_FALSE (stopCalled);
      EXPECT_TRUE (loop.IsRunning ());

      StopAndJoin (loop);
      EXPECT_TRUE (startCalled);
      EXPECT_TRUE (stopCalled);
      EXPECT_FALSE (loop.IsRunning ());

      startCalled = false;
      stopCalled = false;
    }
}

TEST_F (MainLoopTests, NeverRunningIsOk)
{
  MainLoop loop;
  /* loop is never started or stopped.  It is simply destructed at the end
     of the test case, which should be fine.  */
}

TEST_F (MainLoopTests, MustStopBeforeDestruct)
{
  EXPECT_DEATH (
    {
      MainLoop loop;
      RunWithFlags (loop);
    }, "Main loop is still running");
}

TEST_F (MainLoopTests, CannotStartRunning)
{
  EXPECT_DEATH (
    {
      MainLoop loop;
      RunWithFlags (loop);

      std::thread secondStart ([&loop] ()
        {
          MainLoop::Functor nothing = [] () {};
          loop.Run (nothing, nothing);
        });

      SleepSome ();
    }, "Main loop is already running");
}

TEST_F (MainLoopTests, CannotStartAnother)
{
  EXPECT_DEATH (
    {
      MainLoop loop;
      RunWithFlags (loop);

      MainLoop loop2;
      std::thread secondStart ([&loop2] ()
        {
          MainLoop::Functor nothing = [] () {};
          loop2.Run (nothing, nothing);
        });

      SleepSome ();
    }, "Another main loop");
}

} // anonymous namespace
} // namespace internal
} // namespace xaya
