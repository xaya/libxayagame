// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamelogic.hpp"

#include <glog/logging.h>

namespace xaya
{

const std::string&
GameLogic::GetChain () const
{
  CHECK (!chain.empty ());
  return chain;
}

void
GameLogic::SetChain (const std::string& c)
{
  CHECK (chain.empty () || chain == c);
  chain = c;
}

Json::Value
GameLogic::GameStateToJson (const GameStateData& state)
{
  return state;
}

} // namespace xaya
