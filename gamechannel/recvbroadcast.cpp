// Copyright (C) 2019-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "recvbroadcast.hpp"

#include <glog/logging.h>

#include <chrono>
#include <deque>
#include <exception>
#include <utility>

namespace xaya
{

namespace
{

/** Time to wait before retrying after an error in the event loop.  */
constexpr auto RETRY_DELAY = std::chrono::seconds (1);

} // anonymous namespace

ReceivingOffChainBroadcast::ReceivingOffChainBroadcast (
    SynchronisedChannelManager& cm)
  : OffChainBroadcast(cm.Read ()->GetChannelId ()), manager(&cm)
{}

ReceivingOffChainBroadcast::ReceivingOffChainBroadcast (const uint256& i)
  : OffChainBroadcast(i), manager(nullptr)
{
  LOG_FIRST_N (WARNING, 1)
      << "Using ReceivingOffChainBroadcast without ChannelManager,"
         " this should only happen in tests";
}

ReceivingOffChainBroadcast::~ReceivingOffChainBroadcast ()
{
  Stop ();
}

std::vector<std::string>
ReceivingOffChainBroadcast::GetMessages ()
{
  LOG (FATAL)
      << "Subclasses should either override GetMessages()"
         " or ensure that their own Start/Stop event loop does not"
         " call GetMessages";
}

void
ReceivingOffChainBroadcast::Start ()
{
  LOG (INFO) << "Starting default event loop...";
  CHECK (loop == nullptr) << "The event loop is already running";

  stopLoop = false;
  loop = std::make_unique<std::thread> ([this] ()
    {
      RunLoop ();
    });
}

void
ReceivingOffChainBroadcast::Stop ()
{
  if (loop == nullptr)
    return;

  LOG (INFO) << "Stopping default event loop...";
  stopLoop = true;
  loop->join ();
  loop.reset ();
}

void
ReceivingOffChainBroadcast::RunLoop ()
{
  LOG (INFO) << "Running default event loop...";

  /* Messages that have been fetched but not processed yet.  This is local
     to the loop, and lets us retry a message whose processing failed without
     losing it or the rest of the batch.  */
  std::deque<std::string> pending;

  while (!stopLoop)
    {
      try
        {
          if (pending.empty ())
            {
              auto messages = GetMessages ();
              VLOG_IF (1, !messages.empty ())
                  << "Received " << messages.size () << " messages";
              for (auto& msg : messages)
                pending.push_back (std::move (msg));
            }

          while (!pending.empty ())
            {
              FeedMessage (pending.front ());
              pending.pop_front ();
            }
        }
      catch (const std::exception& exc)
        {
          LOG (WARNING)
              << "Error in broadcast event loop, retrying: " << exc.what ();
          std::this_thread::sleep_for (RETRY_DELAY);
        }
    }
}

void
ReceivingOffChainBroadcast::FeedMessage (const std::string& msg)
{
  CHECK (manager != nullptr)
      << "Without ChannelManager, FeedMessage must be overridden";
  auto cmLocked = manager->Access ();
  ProcessIncoming (*cmLocked, msg);
}

} // namespace xaya
