// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_MAINLOOP_HPP
#define XAYAGAME_MAINLOOP_HPP

/* This file is an implementation detail of Game and should not be
   used directly by external code!  */

#include <signal.h>

#include <condition_variable>
#include <functional>
#include <mutex>

namespace xaya
{
namespace internal
{

/**
 * Implementation of the "main loop" logic for Game.  It implements a blocking
 * main loop that can be stopped explicitly (e.g. through incoming RPC calls)
 * or by listening to SIGTERM and SIGINT.
 */
class MainLoop
{

private:

  /** Whether or not the main loop is running.  */
  bool running = false;
  /** Signals the loop to stop.  */
  bool shouldStop;

  /** Mutex guarding the running and stop flags.  */
  std::mutex mut;
  /** Condition variable to signal the main loop to stop.  */
  std::condition_variable cv;

  /** The installed sigaction handler for SIGTERM.  */
  struct sigaction sigtermHandler;

  /**
   * Handles the SIGTERM signal and notifies the running main loop to stop
   * in that case.
   */
  static void HandleInterrupt (int signum);

  friend class MainLoopTests;

public:

  /** Type for start/stop functors (as convenience).  */
  using Functor = std::function<void ()>;

  MainLoop () = default;
  ~MainLoop ();

  MainLoop (const MainLoop&) = delete;
  void operator= (const MainLoop&) = delete;

  /**
   * Run the main loop.  It executes the given "start" function, then
   * blocks until terminated, and then executes "stop".
   */
  void Run (const Functor& start, const Functor& stop);

  /**
   * Signals the main loop to stop if it is running.
   */
  void Stop ();

};

} // namespace internal
} // namespace xaya

#endif // XAYAGAME_MAINLOOP_HPP
