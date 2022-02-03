// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "syncmanager.hpp"

#include <chrono>
#include <thread>

namespace xaya
{

namespace
{

/**
 * Timeout for WaitForChange (i.e. return after this time even if there
 * has not been any change).  Having a timeout in the first place avoids
 * collecting more and more blocked threads in the worst case.
 */
constexpr auto WAITFORCHANGE_TIMEOUT = std::chrono::seconds (5);

} // anonymous namespace

SynchronisedChannelManager::SynchronisedChannelManager (ChannelManager& c)
  : cm(c)
{
  cm.RegisterCallback (*this);
}

SynchronisedChannelManager::~SynchronisedChannelManager ()
{
  StopUpdates ();

  std::unique_lock<std::mutex> lock(mut);
  cm.UnregisterCallback (*this);

  /* Wait for all active waiter threads to finish before we let the instance
     be destructed.  This is just an extra sanity measure and usually updates
     should have been stopped (and event loops calling into waitforchange
     explicitly joined) already before the instance is destructed anyway.

     Using a condition variable here just for signalling possible changes to
     the waiting counter seems overkill in this situation, so we just sleep
     as needed (which in practice won't be at all).  */
  while (waiting > 0)
    {
      LOG_FIRST_N (WARNING, 1)
          << "There are still " << waiting << " waiters active, sleeping";
      lock.unlock ();
      std::this_thread::sleep_for (std::chrono::milliseconds (1));
      lock.lock ();
    }
}

void
SynchronisedChannelManager::StateChanged ()
{
  cvStateChanged.notify_all ();
}

SynchronisedChannelManager::Locked<ChannelManager>
SynchronisedChannelManager::Access ()
{
  return Locked<ChannelManager> (*this);
}

SynchronisedChannelManager::Locked<const ChannelManager>
SynchronisedChannelManager::Read () const
{
  return Locked<const ChannelManager> (*this);
}

void
SynchronisedChannelManager::StopUpdates ()
{
  std::lock_guard<std::mutex> lock(mut);
  stopped = true;
  cvStateChanged.notify_all ();
}

Json::Value
SynchronisedChannelManager::WaitForChange (const int knownVersion) const
{
  std::unique_lock<std::mutex> lock(mut);

  if (knownVersion != WAITFORCHANGE_ALWAYS_BLOCK
          && knownVersion != cm.GetStateVersion ())
    {
      VLOG (1)
          << "Known version " << knownVersion
          << " differs from current one (" << cm.GetStateVersion ()
          << "), returning immediately from WaitForChange";
      return cm.ToJson ();
    }

  if (stopped)
    VLOG (1) << "ChannelManager is stopped, not waiting for changes";
  else
    {
      VLOG (1) << "Waiting for state change on condition variable...";
      ++waiting;
      cvStateChanged.wait_for (lock, WAITFORCHANGE_TIMEOUT);
      CHECK_GT (waiting, 0);
      --waiting;
      VLOG (1) << "Potential state change detected in WaitForChange";
    }

  return cm.ToJson ();
}

} // namespace xaya
