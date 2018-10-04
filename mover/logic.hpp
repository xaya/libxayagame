// Copyright (C) 2018 The Xaya developers
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

private:

  /**
   * Parses a move object into direction and number of steps.  Returns false
   * if the move is somehow invalid.
   */
  static bool ParseMove (const Json::Value& obj,
                         proto::Direction& dir, unsigned& steps);

  friend class ParseMoveTests;

public:

  xaya::GameStateData GetInitialState (unsigned& height,
                                       std::string& hashHex) override;

  xaya::GameStateData ProcessForward (const xaya::GameStateData& oldState,
                                      const Json::Value& blockData,
                                      xaya::UndoData& undo) override;

  xaya::GameStateData ProcessBackwards (const xaya::GameStateData& newState,
                                        const Json::Value& blockData,
                                        const xaya::UndoData& undo) override;

  Json::Value GameStateToJson (const xaya::GameStateData& state) override;

};

} // namespace mover

#endif // MOVER_LOGIC_HPP
