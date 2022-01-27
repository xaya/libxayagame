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

} // namespace xaya
