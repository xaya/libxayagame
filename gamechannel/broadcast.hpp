// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_BROADCAST_HPP
#define GAMECHANNEL_BROADCAST_HPP

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayautil/uint256.hpp>

#include <set>
#include <string>

namespace xaya
{

class ChannelManager;

/**
 * This class handles the off-chain broadcast of messages within a channel.
 * It contains some general logic, but also concrete implementations for
 * exchanging messages (e.g. via a server, XMPP, IRC, P2P, ...) have to
 * subclass OffChainBroadcaster and implement their logic.
 *
 * The core interface in this class provides functionality to send
 * mossages (local moves to everyone else in the channel).  This is what
 * gets directly used by the ChannelManager and must be provided to it.
 *
 * Receiving messages and feeding them into ChannelManager::ProcessOffChain
 * is a separate task, which is not directly handled by this class.
 */
class OffChainBroadcast
{

private:

  /** The channel ID this is for.  */
  const uint256 id;

  /**
   * The list of channel participants (names without p/ prefix).  This is
   * updated to the latest known on-chain state with channel reinitialisations.
   * It may be used by concrete implementations for sending messages to all
   * known participants.
   */
  std::set<std::string> participants;

protected:

  /**
   * Sends a given encoded message to all participants in the channel.
   */
  virtual void SendMessage (const std::string& msg) = 0;

public:

  /**
   * Constructs an instance for the given channel ID.
   */
  explicit OffChainBroadcast (const uint256& i)
    : id(i)
  {}

  virtual ~OffChainBroadcast () = default;

  OffChainBroadcast () = delete;
  OffChainBroadcast (const OffChainBroadcast&) = delete;
  void operator= (const OffChainBroadcast&) = delete;

  /**
   * Returns the ID of the channel for which this is.  Can be used by
   * implementations if they need it.
   */
  const uint256&
  GetChannelId () const
  {
    return id;
  }

  /**
   * Sends a new state (presumably after the player made a move) to all
   * channel participants.
   */
  void SendNewState (const std::string& reinitId,
                     const proto::StateProof& proof);

  /**
   * Returns the current list of participants.  This may be used by
   * subclasses for their implementation of SendMessage.
   */
  const std::set<std::string>&
  GetParticipants () const
  {
    return participants;
  }

  /**
   * Updates the list of channel participants when the on-chain state changes.
   */
  void SetParticipants (const proto::ChannelMetadata& meta);

  /**
   * Decodes a message and feeds the corresponding state into the
   * ChannelManager's ProcessOffChain method.  It is assumed that
   * this instance is used as OffChainBroadcast on the channel manager m.
   */
  void ProcessIncoming (ChannelManager& m, const std::string& msg) const;

};

} // namespace xaya

#endif // GAMECHANNEL_BROADCAST_HPP
