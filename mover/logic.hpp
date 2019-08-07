// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MOVER_LOGIC_HPP
#define MOVER_LOGIC_HPP

#include "proto/mover.pb.h"

#include "xayagame/gamelogic.hpp"
#include "xayagame/storage.hpp"

#include <json/json.h>

#include <string>

namespace mover
{

/**
 * The actual implementation of the game rules.
 */
class MoverLogic : public xaya::GameLogic
{

protected:

  xaya::GameStateData GetInitialStateInternal (unsigned& height,
                                               std::string& hashHex) override;

  xaya::GameStateData ProcessForwardInternal (
      const xaya::GameStateData& oldState, const Json::Value& blockData,
      xaya::UndoData& undo) override;

  xaya::GameStateData ProcessBackwardsInternal (
      const xaya::GameStateData& newState, const Json::Value& blockData,
      const xaya::UndoData& undo) override;

public:

  Json::Value GameStateToJson (const xaya::GameStateData& state) override;

};

} // namespace mover

#endif // MOVER_LOGIC_HPP
