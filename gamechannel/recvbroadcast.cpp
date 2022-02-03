// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "recvbroadcast.hpp"

#include <glog/logging.h>

namespace xaya
{

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
  while (!stopLoop)
    {
      const auto messages = GetMessages ();
      VLOG_IF (1, !messages.empty ())
          << "Received " << messages.size () << " messages";
      for (const auto& msg : messages)
        FeedMessage (msg);
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
