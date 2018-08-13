// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include <glog/logging.h>

namespace xaya
{

Game::Game (const std::string& id)
  : gameId(id)
{}

Game::~Game ()
{}

void
Game::Run ()
{
  internal::MainLoop::Functor startAction = [this] () {};
  internal::MainLoop::Functor stopAction = [this] () {};
  mainLoop.Run (startAction, stopAction);
}

} // namespace xaya
