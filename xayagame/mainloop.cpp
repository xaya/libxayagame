// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mainloop.hpp"

#include <glog/logging.h>

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

#ifndef _WIN32
  const int signals[] = {SIGTERM, SIGINT};
#endif // !_WIN32
  {
    std::lock_guard<std::mutex> instanceLock(instanceForSignalsMutex);
    CHECK (instanceForSignals == nullptr)
        << "Another main loop is already running";
    instanceForSignals = this;

#ifndef _WIN32
    for (const int sig : signals)
      if (sigaction (sig, &sigtermHandler, nullptr) != 0)
        LOG (FATAL) << "Installing signal handler failed for signal " << sig;
#endif // !_WIN32
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

#ifndef _WIN32
    for (const int sig : signals)
      if (sigaction (sig, nullptr, nullptr) != 0)
        LOG (FATAL) << "Uninstalling signal handler failed for signal " << sig;
#endif // !_WIN32
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
MainLoop::HandleInterrupt (const int signum)
{
  std::lock_guard<std::mutex> lock(instanceForSignalsMutex);
  if (instanceForSignals == nullptr)
    return;
  instanceForSignals->Stop ();
}

} // namespace internal
} // namespace xaya
