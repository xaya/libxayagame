// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_RECVBROADCAST_HPP
#define GAMECHANNEL_RECVBROADCAST_HPP

#include "broadcast.hpp"
#include "channelmanager.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace xaya
{

/**
 * A subclass of OffChainBroadcast, which also takes care of an event loop
 * for receiving messages.
 *
 * There are two general architectures that implementations can use for
 * that:  If they have their own event loop, then they should override the
 * Start and Stop methods, and feed messages they receive to FeedMessage.
 *
 * Alternatively, the default implementation of Start and Stop will run
 * a waiting loop in a new thread, and repeatedly call GetMessages to
 * retrieve the next message in a blocking call.
 */
class ReceivingOffChainBroadcast : public OffChainBroadcast
{

private:

  /**
   * The ChannelManager instance that is updated with received messages.
   *
   * For testing purposes it can be null, in which case we require that
   * FeedMessage is overridden in a subclass to handle the messages directly.
   */
  SynchronisedChannelManager* manager;

  /** The currently running wait loop, if any.  */
  std::unique_ptr<std::thread> loop;

  /** If set to true, signals the loop to stop.  */
  std::atomic<bool> stopLoop;

  /**
   * Runs the default event loop, waiting for messages.
   */
  void RunLoop ();

protected:

  /**
   * Constructs an instance without a ChannelManager but the given explicit
   * channel ID.  This can be used for testing broadcast implementations;
   * in those tests, the FeedMessage method must be overridden to handle
   * messages directly.
   */
  explicit ReceivingOffChainBroadcast (const uint256& i);

  /**
   * Processes a message retrieved through the broadcast channel.  If the
   * instance has been created with a channel ID and not a ChannelManager
   * (for testing), then subclasses must explicitly override this method
   * to handle messages themselves.
   */
  virtual void FeedMessage (const std::string& msg);

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

  /**
   * Constructs an instance for normal use.  It will feed messages into
   * the given ChannelManager.
   */
  explicit ReceivingOffChainBroadcast (SynchronisedChannelManager& cm);

  ~ReceivingOffChainBroadcast ();

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

#endif // GAMECHANNEL_RECVBROADCAST_HPP
