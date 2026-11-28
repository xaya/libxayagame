// Copyright (C) 2018-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mainloop.hpp"

#include <glog/logging.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>

namespace xaya
{
namespace internal
{

MainLoop::MainLoop ()
{
#ifndef _WIN32
  std::memset (&sigtermHandler, 0, sizeof (sigtermHandler));
  sigtermHandler.sa_handler = &MainLoop::HandleInterrupt;
#endif // !_WIN32
}

MainLoop::~MainLoop ()
{
  CHECK (!IsRunning ()) << "Main loop is still running, cannot destroy it";
}

bool
MainLoop::IsRunning () const
{
  std::lock_guard<std::mutex> lock(mut);
  return running;
}

namespace
{

/** Whether a MainLoop is currently running.  */
std::atomic<bool> mainLoopActive = false;

/** Set by the interrupt handler to request a stop.  */
std::atomic<bool> stopRequested = false;

/**
 * Helper class to mark a MainLoop as active with RAII.
 */
class MainLoopActiveGuard
{

public:

  MainLoopActiveGuard ()
  {
    CHECK (!mainLoopActive.exchange (true))
        << "Another main loop is already running";
  }

  ~MainLoopActiveGuard ()
  {
    mainLoopActive = false;
  }

  MainLoopActiveGuard (const MainLoopActiveGuard&) = delete;
  void operator= (const MainLoopActiveGuard&) = delete;

};

static_assert (std::atomic<bool>::is_always_lock_free,
               "The signal handler requires lock-free atomics");

#ifndef _WIN32

constexpr std::array<int, 2> interruptSignals = {SIGTERM, SIGINT};

/**
 * Helper class to install the signal handlers and restore the previous
 * dispositions with RAII.
 */
class SignalHandlerGuard
{

private:

  std::array<struct sigaction, interruptSignals.size ()> oldActions;
  std::array<bool, interruptSignals.size ()> installed{};

  void
  Restore ()
  {
    for (size_t i = 0; i < interruptSignals.size (); ++i)
      if (installed[i])
        {
          if (sigaction (interruptSignals[i], &oldActions[i], nullptr) != 0)
            LOG (ERROR)
                << "Restoring signal handler failed for signal "
                << interruptSignals[i];
          installed[i] = false;
        }
  }

public:

  explicit SignalHandlerGuard (const struct sigaction& handler)
  {
    for (size_t i = 0; i < interruptSignals.size (); ++i)
      {
        if (sigaction (interruptSignals[i], &handler, &oldActions[i]) != 0)
          {
            Restore ();
            LOG (FATAL)
                << "Installing signal handler failed for signal "
                << interruptSignals[i];
          }
        installed[i] = true;
      }
  }

  ~SignalHandlerGuard ()
  {
    Restore ();
  }

  SignalHandlerGuard (const SignalHandlerGuard&) = delete;
  void operator= (const SignalHandlerGuard&) = delete;

};

#endif // !_WIN32

} // anonymous namespace

void
MainLoop::Run (const Functor& start, const Functor& stop)
{
  std::unique_lock<std::mutex> mainLoopLock(mut);
  CHECK (!running) << "Main loop is already running, cannot start it again";

  stopRequested = false;

  MainLoopActiveGuard activeGuard;
#ifndef _WIN32
  SignalHandlerGuard signalGuard(sigtermHandler);
#endif // !_WIN32

  shouldStop = false;
  running = true;

  bool needStop = true;
  try
    {
      LOG (INFO) << "Starting main loop";
      start ();

      while (!shouldStop)
        {
          const auto pred = [this] ()
             {
               return shouldStop
                  || stopRequested.load (std::memory_order_acquire);
             };
          cv.wait_for (mainLoopLock, std::chrono::milliseconds (100), pred);

          if (stopRequested.load (std::memory_order_acquire))
            shouldStop = true;
        }

      LOG (INFO) << "Stopping main loop";

      /* If the stop functor itself throws, do not call it a second time
         from the catch block below.  */
      needStop = false;
      stop ();
    }
  catch (...)
    {
      if (needStop)
        try
          {
            /* A partially-completed start may already have launched workers,
               so make sure they are shut down again.  */
            stop ();
          }
        catch (...)
          {
            LOG (ERROR) << "Exception while stopping partially started loop";
          }

      running = false;
      throw;
    }

  running = false;
}

void
MainLoop::Stop ()
{
  std::lock_guard<std::mutex> lock(mut);
  shouldStop = true;
  cv.notify_all ();
}

void
MainLoop::HandleInterrupt (const int signum)
{
  stopRequested.store (true, std::memory_order_relaxed);
}

} // namespace internal
} // namespace xaya
