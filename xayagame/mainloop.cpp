// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mainloop.hpp"

#include <glog/logging.h>

namespace xaya
{
namespace internal
{

MainLoop::~MainLoop ()
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (!running) << "Main loop is still running, cannot destroy it";
}

namespace
{

/**
 * Helper class to run start/stop functions with RAII.
 */
template <typename F>
  class StartStopRunner
{

private:

  const F& startFunc;
  const F& stopFunc;

public:

  explicit StartStopRunner (const F& start, const F& stop)
    : startFunc(start), stopFunc(stop)
  {
    startFunc ();
  }

  ~StartStopRunner ()
  {
    stopFunc ();
  }

};

/**
 * Global variable holding the instance of MainLoop which is currently running
 * thus handled SIGTERM's are meant.
 */
MainLoop* instanceForSignals = nullptr;

/** Mutex guarding instanceForSignals.  */
std::mutex instanceForSignalsMutex;

} // anonymous namespace

void
MainLoop::Run (const Functor& start, const Functor& stop)
{
  std::unique_lock<std::mutex> mainLoopLock(mut);
  CHECK (!running) << "Main loop is already running, cannot start it again";

  const int signals[] = {SIGTERM, SIGINT};
  {
    std::lock_guard<std::mutex> instanceLock(instanceForSignalsMutex);
    CHECK (instanceForSignals == nullptr)
        << "Another main loop is already running";
    instanceForSignals = this;
    sigtermHandler.sa_handler = &MainLoop::HandleInterrupt;
    for (const int sig : signals)
      if (sigaction (sig, &sigtermHandler, nullptr) != 0)
        LOG (FATAL) << "Installing signal handler failed for signal " << sig;
  }

  shouldStop = false;
  running = true;
  {
    LOG (INFO) << "Starting main loop";
    StartStopRunner<Functor> startStop(start, stop);
    while (!shouldStop)
      cv.wait (mainLoopLock);
    LOG (INFO) << "Stopping main loop";
  }
  running = false;

  {
    std::lock_guard<std::mutex> instanceLock(instanceForSignalsMutex);
    instanceForSignals = nullptr;
    for (const int sig : signals)
      if (sigaction (sig, nullptr, nullptr) != 0)
        LOG (FATAL) << "Uninstalling signal handler failed for signal " << sig;
  }
}

void
MainLoop::Stop ()
{
  std::lock_guard<std::mutex> lock(mut);
  shouldStop = true;
  cv.notify_all ();
}

void
MainLoop::HandleInterrupt (int signum)
{
  std::lock_guard<std::mutex> lock(instanceForSignalsMutex);
  if (instanceForSignals == nullptr)
    return;
  instanceForSignals->Stop ();
}

} // namespace internal
} // namespace xaya
