// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_ZMQSUBSCRIBER_HPP
#define XAYAGAME_ZMQSUBSCRIBER_HPP

/* This file is an implementation detail of Game and should not be
   used directly by external code!  */

#include <zmq.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace xaya
{
namespace internal
{

/**
 * The Game subsystem that implements the ZMQ subscriber to the Xaya daemon's
 * game-block-* notifications (for a particular game ID).
 */
class ZmqSubscriber
{

private:

  /** The subscribed game ID.  */
  const std::string gameId;

  /** The ZMQ endpoint to connect to.  */
  std::string addr;
  /** The ZMQ context that is used by the this instance.  */
  zmq::context_t ctx;
  /** The ZMQ socket used to subscribe to the Xaya daemon, if connected.  */
  std::unique_ptr<zmq::socket_t> socket;

  /** The running ZMQ listener thread, if any.  */
  std::unique_ptr<std::thread> listener;

  /** Signals the listener to stop.  */
  bool shouldStop;
  /** Mutex guarding shouldStop.  */
  std::mutex mut;

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

public:

  explicit ZmqSubscriber (const std::string& id);
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
   * Returns true if the ZMQ subscriber is currently running.
   */
  bool
  IsRunning () const
  {
    return listener != nullptr;
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
