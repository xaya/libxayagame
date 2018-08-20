// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_GAME_HPP
#define XAYAGAME_GAME_HPP

#include "mainloop.hpp"
#include "zmqsubscriber.hpp"

#include <json/json.h>

#include <string>

namespace xaya
{

/**
 * The main class implementing a game on the Xaya platform.  It handles the
 * ZMQ and RPC communication with the Xaya daemon as well as the RPC interface
 * of the game itself.
 *
 * To implement a game, create a subclass that overrides the pure virtual
 * methods with the actual game logic and then instantiate and Run() it
 * from the binary's main().
 */
class Game : private internal::ZmqListener
{

private:

  /** This game's game ID.  */
  const std::string gameId;

  /** The ZMQ subscriber.  */
  internal::ZmqSubscriber zmq;

  /** The main loop.  */
  internal::MainLoop mainLoop;

  void BlockAttach (const std::string& id, const Json::Value& data,
                    bool seqMismatch) override;
  void BlockDetach (const std::string& id, const Json::Value& data,
                    bool seqMismatch) override;

protected:

  explicit Game (const std::string& id);
  virtual ~Game () = default;

  Game () = delete;
  Game (const Game&) = delete;
  void operator= (const Game&) = delete;

public:

  /**
   * Sets the ZMQ endpoint that will be used to connect to the ZMQ interface
   * of the Xaya daemon.  Must not be called anymore after StartZmq() or
   * Run() have been called.
   */
  void
  SetZmqEndpoint (const std::string& addr)
  {
    zmq.SetEndpoint (addr);
  }

  /**
   * Starts the ZMQ subscriber in a new thread.  Must only be called after
   * the ZMQ endpoint has been configured, and must not be called when
   * ZMQ is already running.
   */
  void
  StartZmq ()
  {
    zmq.Start ();
  }

  /**
   * Stops the ZMQ subscriber.  Must only be called if it is currently running.
   */
  void
  StopZmq ()
  {
    zmq.Stop ();
  }

  /**
   * Runs the main event loop for the Game.  This starts all configured
   * subsystems (ZMQ, RPC server) and blocks the calling thread until
   * a stop of the server is requested.  Then those subsystems are stopped
   * again and the method returns.
   */
  void Run ();

};

} // namespace xaya

#endif // XAYAGAME_GAME_HPP
