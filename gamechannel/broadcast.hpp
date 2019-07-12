// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_BROADCAST_HPP
#define GAMECHANNEL_BROADCAST_HPP

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayautil/uint256.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace xaya
{

class ChannelManager;

/**
 * This class handles the off-chain broadcast of messages within a channel.
 * It contains some general logic, but also concrete implementations for
 * exchanging messages (e.g. via a server, XMPP, IRC, P2P, ...) have to
 * subclass OffChainBroadcaster and implement their logic.
 *
 * For sending messages (local moves to everyone else in the channel),
 * subclasses need to implement the SendMessage function.  Receiving
 * messages is more complex, as that needs to take care of waiting for
 * incoming messages.
 *
 * There are two general architectures that implementations can use for
 * that:  If they have their own event loop, then they should override the
 * Start and Stop methods, and feed messages they receive to FeedMessage.
 *
 * Alternatively, the default implementation of Start and Stop will run
 * a waiting loop in a new thread, and repeatedly call GetMessages to
 * retrieve the next message in a blocking call.
 */
class OffChainBroadcast
{

private:

  /** The ChannelManager instance that is updated with received messges.  */
  ChannelManager& manager;

  /**
   * The list of channel participants (names without p/ prefix).  This is
   * updated to the latest known on-chain state with channel reinitialisations.
   * It may be used by concrete implementations for sending messages to all
   * known participants.
   */
  std::set<std::string> participants;

  /**
   * Lock for shared state, in particular participants.  This lock is held
   * while SendNewState is running.  It is not held through the receive
   * cycle, though.
   */
  mutable std::mutex mut;

  /** The currently running wait loop, if any.  */
  std::unique_ptr<std::thread> loop;

  /** If set to true, signals the loop to stop.  */
  std::atomic<bool> stopLoop;

  /**
   * Runs the default event loop, waiting for messages.
   */
  void RunLoop ();

protected:

  explicit OffChainBroadcast (ChannelManager& cm)
    : manager(cm)
  {}

  /**
   * Returns the current list of participants.  This may be used by
   * subclasses for their implementation of SendMessage.  While SendMessage
   * is running, the participants are properly synchronised.  At any other
   * time, calling this function may lead to race conditions.
   */
  const std::set<std::string>&
  GetParticipants () const
  {
    return participants;
  }

  /**
   * Returns the ID of the channel for which this is.  Can be used by
   * implementations if they need it.
   */
  const uint256& GetChannelId () const;

  /**
   * Sends a given encoded message to all participants in the channel.
   * This function may be called from different threads, but it is guaranteed
   * that it is only called by one concurrent thread at any time.
   */
  virtual void SendMessage (const std::string& msg) = 0;

  /**
   * Processes a message retrieved through the broadcast channel.
   */
  void FeedMessage (const std::string& msg);

  /**
   * Tries to retrieve more messages from the underlying communication system,
   * blocking until one is available.  If subclasses want to make use of
   * the default Start/Stop and event loop, then they should override this
   * method.  Calls should never block for an unlimited amount of time,
   * but time out and return an empty vector after some delay.
   *
   * It is guaranteed that this function is only called by one concurrent
   * thread at any given time (when used in combination with the default
   * Start/Stop event loop).
   */
  virtual std::vector<std::string> GetMessages ();

public:

  virtual ~OffChainBroadcast ();

  OffChainBroadcast () = delete;
  OffChainBroadcast (const OffChainBroadcast&) = delete;
  void operator= (const OffChainBroadcast&) = delete;

  /**
   * Sends a new state (presumably after the player made a move) to all
   * channel participants.
   */
  void SendNewState (const std::string& reinitId,
                     const proto::StateProof& proof);

  /**
   * Updates the list of channel participants when the on-chain state changes.
   */
  void SetParticipants (const proto::ChannelMetadata& meta);

  /**
   * Starts an event loop listening for new messages and feeding them into
   * FeedMessage as received.  Subclasses can override this (together with
   * Stop) to provide their own event loop.  The default implementation will
   * start a new thread that just calls GetMessages repeatedly.
   */
  virtual void Start ();

  /**
   * Stops the event loop if one is running.  If subclasses override this
   * method, they need to ensure that it is fine to call it even if the event
   * loop is not running at the moment.
   */
  virtual void Stop ();

};

} // namespace xaya

#endif // GAMECHANNEL_BROADCAST_HPP
