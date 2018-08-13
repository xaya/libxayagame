// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include <glog/logging.h>

namespace xaya
{

Game::Game (const std::string& id)
  : gameId(id), zmq(gameId)
{}

void
Game::Run ()
{
  const bool zmqStarted = zmq.IsEndpointSet ();
  internal::MainLoop::Functor startAction = [this, zmqStarted] ()
    {
      if (zmqStarted)
        StartZmq ();
      else
        LOG (INFO)
            << "No ZMQ endpoint is set, not starting ZMQ from Game::Run()";
    };
  internal::MainLoop::Functor stopAction = [this, zmqStarted] ()
    {
      if (zmqStarted)
        StopZmq ();
    };
  mainLoop.Run (startAction, stopAction);
}

} // namespace xaya
