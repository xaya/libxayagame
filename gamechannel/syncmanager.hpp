// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_SYNCMANAGER_HPP
#define GAMECHANNEL_SYNCMANAGER_HPP

#include "channelmanager.hpp"

#include <json/json.h>

#include <condition_variable>
#include <mutex>

namespace xaya
{

/**
 * A ChannelManager reference together with a mutex, so that it can be accessed
 * from multiple threads (e.g. update event loops and an RPC server).
 * This class also supports a waitforchange-like interface for handling
 * state changes.
 */
class SynchronisedChannelManager : private ChannelManager::Callbacks
{

private:

  template <typename T>
    class Locked;

  /** The actual ChannelManager instance used.  */
  ChannelManager& cm;

  /**
   * Mutex for synchronising the internal channel manager.  Also used as
   * lock for the waitforchange condition variable.
   */
  mutable std::mutex mut;

  /**
   * Condition variable that gets signalled when the state is changed due
   * to on-chain updates, off-chain updates or local moves.  This is used
   * for waitforchange.
   */
  mutable std::condition_variable cvStateChanged;

  /**
   * If set to true, then future waitforchange calls will not block anymore.
   * We use this to ensure we can properly wake all waiters up when shutting
   * down, without them re-calling.
   */
  bool stopped = false;

  /**
   * Number of currently blocked waiter calls.
   */
  mutable unsigned waiting = 0;

  void StateChanged () override;

public:

  /**
   * Special value for the known version in WaitForChange that tells the
   * function to always block.
   */
  static constexpr int WAITFORCHANGE_ALWAYS_BLOCK = 0;

  explicit SynchronisedChannelManager (ChannelManager& c);
  ~SynchronisedChannelManager ();

  SynchronisedChannelManager () = delete;
  SynchronisedChannelManager (const SynchronisedChannelManager&) = delete;
  void operator= (const SynchronisedChannelManager&) = delete;

  /**
   * Returns a "locked instance" of the underlying ChannelManager.  This is
   * a movable instance that holds the underlying mutex while it exists
   * (like a std::unique_lock) and can be dereferenced to yield the
   * ChannelManager.
   *
   * This method returns a mutable instance of the ChannelManager.
   */
  Locked<ChannelManager> Access ();

  /**
   * Returns a read-only locked ChannelManager (similar to Access).
   */
  Locked<const ChannelManager> Read () const;

  /**
   * Disables processing of updates in the future.  This should be called
   * when shutting down the channel daemon.  It makes sure that all waiting
   * callers to WaitForChange are woken up, and no more callers will block
   * in the future.  Thus, this mechanism ensures that we can properly
   * shut down WaitForChange.
   *
   * This function must be called before a ChannelManager instance is
   * destructed.  Otherwise the destructor will CHECK-fail.
   */
  void StopUpdates ();

  /**
   * Blocks the calling thread until the state of the channel has (probably)
   * been changed.  This can be used by frontends to implement long-polling
   * RPC methods like waitforchange.  Note that the function may return
   * spuriously even if there is no new state.
   *
   * If the passed-in version is different from the current state version
   * already when starting the call, the function returns immediately.  Ideally,
   * clients should pass in the version they currently know (as returned
   * in the JSON state in "version"), so that we can avoid race conditions
   * when a change happens between two calls to WaitForChange.
   *
   * When WAITFORCHANGE_ALWAYS_BLOCK is passed as the known version, then the
   * function will always block until the next update.
   *
   * On return, the current (i.e. likely new) state is returned in the same
   * format as ToJson() would return.
   */
  Json::Value WaitForChange (int knownVersion) const;

};

/**
 * A "locked" ChannelManager instance.  While an object exists, it will
 * hold the mutex of an underlying SynchronisedChannelManager, and give
 * access to its ChannelManager.
 *
 * The type T is either "ChannelManager" or "const ChannelManager".
 */
template <typename T>
  class SynchronisedChannelManager::Locked
{

private:

  /** The underlying instance that can be accessed.  */
  T& instance;

  /** The lock on the mutex held by this instance.  */
  std::unique_lock<std::mutex> lock;

public:

  /**
   * Constructs a new instance, based on the underlying
   * SynchronisedChannelManager instance.
   */
  template <typename CM>
    explicit Locked (CM& underlying)
    : instance(underlying.cm), lock(underlying.mut)
  {}

  Locked (Locked&&) = default;
  Locked& operator= (Locked&&) = default;

  Locked (const Locked&) = delete;
  void operator= (const Locked&) = delete;

  T*
  operator-> ()
  {
    return &instance;
  }

  T&
  operator* ()
  {
    return instance;
  }

};

} // namespace xaya

#endif // GAMECHANNEL_SYNCMANAGER_HPP
