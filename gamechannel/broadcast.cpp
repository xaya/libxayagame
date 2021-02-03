// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "broadcast.hpp"

#include "channelmanager.hpp"
#include "proto/broadcast.pb.h"

#include <xayautil/base64.hpp>

#include <glog/logging.h>

#include <sstream>

namespace xaya
{

OffChainBroadcast::OffChainBroadcast (ChannelManager& cm)
  : manager(&cm), id(manager->GetChannelId ())
{}

OffChainBroadcast::OffChainBroadcast (const uint256& i)
  : manager(nullptr), id(i)
{
  LOG_FIRST_N (WARNING, 1)
      << "Using OffChainBroadcast without ChannelManager,"
         " this should only happen in tests";
}

OffChainBroadcast::~OffChainBroadcast ()
{
  Stop ();
}

void
OffChainBroadcast::SetParticipants (const proto::ChannelMetadata& meta)
{
  std::set<std::string> newParticipants;
  for (const auto& p : meta.participants ())
    newParticipants.insert (p.name ());

  std::lock_guard<std::mutex> lock(mut);

  if (participants != newParticipants)
    {
      std::ostringstream msg;
      msg << "Updating list of participants in broadcast channel to:";
      for (const auto& p : newParticipants)
        msg << " " << p;
      LOG (INFO) << msg.str ();
    }

  participants = std::move (newParticipants);
}

std::vector<std::string>
OffChainBroadcast::GetMessages ()
{
  LOG (FATAL)
      << "Subclasses should either override GetMessages()"
         " or ensure that their own Start/Stop event loop does not"
         " call GetMessages";
}

void
OffChainBroadcast::Start ()
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
OffChainBroadcast::Stop ()
{
  if (loop == nullptr)
    return;

  LOG (INFO) << "Stopping default event loop...";
  stopLoop = true;
  loop->join ();
  loop.reset ();
}

void
OffChainBroadcast::RunLoop ()
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
OffChainBroadcast::SendNewState (const std::string& reinitId,
                                 const proto::StateProof& proof)
{
  VLOG (1) << "Broadcasting new state for reinit " << EncodeBase64 (reinitId);

  proto::BroadcastMessage pb;
  pb.set_reinit (reinitId);
  *pb.mutable_proof () = proof;

  std::string msg;
  CHECK (pb.SerializeToString (&msg));

  std::lock_guard<std::mutex> lock(mut);
  SendMessage (msg);
}

void
OffChainBroadcast::FeedMessage (const std::string& msg)
{
  CHECK (manager != nullptr)
      << "Without ChannelManager, FeedMessage must be overridden";
  VLOG (1) << "Processing received broadcast message...";

  proto::BroadcastMessage pb;
  if (!pb.ParseFromString (msg))
    {
      LOG (ERROR)
          << "Failed to parse BroadcastMessage proto from received data";
      return;
    }

  manager->ProcessOffChain (pb.reinit (), pb.proof ());
}

} // namespace xaya
