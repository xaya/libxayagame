// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_ZMQSUBSCRIBER_HPP
#define XAYAGAME_ZMQSUBSCRIBER_HPP

/* This file is an implementation detail of Game and should not be
   used directly by external code!  */

#include <zmq.hpp>

#include <json/json.h>

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace xaya
{

class GameTests;

namespace internal
{

/**
 * Interface that is used to receive updates from the ZmqSubscriber class.
 */
class ZmqListener
{

public:

  virtual ~ZmqListener () = default;

  /**
   * Callback for attached blocks.  It receives the game ID, associated JSON
   * data for the ZMQ notification and whether or not the sequence number
   * was mismatched.  The very first notification for each topic is seen as
   * "mismatched" sequence number as well.
   */
  virtual void BlockAttach (const std::string& gameId,
                            const Json::Value& data, bool seqMismatch) = 0;

  /**
   * Callback for detached blocks, receives same arguments as GameBlockAttach.
   */
  virtual void BlockDetach (const std::string& gameId,
                            const Json::Value& data, bool seqMismatch) = 0;

};

/**
 * The Game subsystem that implements the ZMQ subscriber to the Xaya daemon's
 * game-block-* notifications (for a particular game ID).
 */
class ZmqSubscriber
{

private:

  /** The ZMQ endpoint to connect to.  */
  std::string addr;
  /** The ZMQ context that is used by the this instance.  */
  zmq::context_t ctx;
  /** The ZMQ socket used to subscribe to the Xaya daemon, if connected.  */
  std::unique_ptr<zmq::socket_t> socket;

  /** Game IDs and associated listeners.  */
  std::unordered_multimap<std::string, ZmqListener*> listeners;

  /** Last sequence numbers for each topic.  */
  std::unordered_map<std::string, uint32_t> lastSeq;

  /** The running ZMQ listener thread, if any.  */
  std::unique_ptr<std::thread> worker;

  /** Signals the listener to stop.  */
  bool shouldStop;
  /** Mutex guarding shouldStop.  */
  std::mutex mut;

  /**
   * Special flag for testing:  If true, then the listening thread stops
   * without actually reading messages.
   */
  bool noListeningForTesting = false;

  /**
   * Receives a three-part message sent by the Xaya daemon (consisting
   * of topic and payload as strings as well as the serial number).  Returns
   * false if the socket was closed or the subscriber stopped, and errors out
   * on any other errors.
   */
  bool ReceiveMultiparts (std::string& topic, std::string& payload,
                          uint32_t& seq);

  /**
   * Listens on the ZMQ socket for messages until the socket is closed.
   */
  static void Listen (ZmqSubscriber* self);

  friend class BasicZmqSubscriberTests;
  friend class xaya::GameTests;

public:

  ZmqSubscriber () = default;
  ~ZmqSubscriber ();

  ZmqSubscriber (const ZmqSubscriber&) = delete;
  void operator= (const ZmqSubscriber&) = delete;

  /**
   * Sets the ZMQ endpoint that will be used to connect to the ZMQ interface
   * of the Xaya daemon.  Must not be called anymore after Start() has been
   * called.
   */
  void SetEndpoint (const std::string& address);

  /**
   * Returns whether the endpoint is set.
   */
  bool
  IsEndpointSet () const
  {
    return !addr.empty ();
  }

  /**
   * Adds a new listener for the given game ID.  Must not be called when
   * the subscriber is running.
   */
  void AddListener (const std::string& gameId, ZmqListener* listener);

  /**
   * Returns true if the ZMQ subscriber is currently running.
   */
  bool
  IsRunning () const
  {
    return worker != nullptr;
  }

  /**
   * Starts the ZMQ subscriber in a new thread.  Must only be called after
   * the ZMQ endpoint has been configured, and must not be called when
   * ZMQ is already running.
   */
  void Start ();

  /**
   * Stops the ZMQ subscriber.  Must only be called if it is currently running.
   */
  void Stop ();

};

} // namespace internal
} // namespace xaya

#endif // XAYAGAME_ZMQSUBSCRIBER_HPP
