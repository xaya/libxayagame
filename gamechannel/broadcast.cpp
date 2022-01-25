// Copyright (C) 2019-2022 The Xaya developers
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

/* ************************************************************************** */

void
OffChainBroadcast::SetParticipants (const proto::ChannelMetadata& meta)
{
  std::set<std::string> newParticipants;
  for (const auto& p : meta.participants ())
    newParticipants.insert (p.name ());

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

  SendMessage (msg);
}

void
OffChainBroadcast::ProcessIncoming (ChannelManager& m,
                                    const std::string& msg) const
{
  VLOG (1) << "Processing received broadcast message...";
  CHECK (m.GetChannelId () == id) << "Channel ID mismatch";

  proto::BroadcastMessage pb;
  if (!pb.ParseFromString (msg))
    {
      LOG (ERROR)
          << "Failed to parse BroadcastMessage proto from received data";
      return;
    }

  m.ProcessOffChain (pb.reinit (), pb.proof ());
}

/* ************************************************************************** */

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

/* ************************************************************************** */

} // namespace xaya
