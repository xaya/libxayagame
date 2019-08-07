// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MOVER_PENDING_HPP
#define MOVER_PENDING_HPP

#include "xayagame/pendingmoves.hpp"
#include "xayagame/storage.hpp"

#include <json/json.h>

namespace mover
{

/**
 * PendingMoveProcessor for mover.  In the pending state, we keep track of
 * the latest update for each name and the projected target of the movement,
 * i.e. what it would be when all pending moves were confirmed.
 */
class PendingMoves : public xaya::PendingMoveProcessor
{

private:

  /**
   * The current pending state.  For simplicity we keep it already as a
   * JSON object (indexed by the player names), as we need no further processing
   * of the data except replacing entries and returning the JSON.
   */
  Json::Value pending;

  /**
   * Processes a new pending move, but gets passed the current game state
   * instead of retrieving it from GetConfirmedState.  We use that for testing,
   * so that we can specify the current game state without going through the
   * upstream PendingMoveProcessor logic.
   */
  void AddPendingMoveInternal (const xaya::GameStateData& state,
                               const Json::Value& mv);

  friend class PendingMoveTests;

protected:

  void Clear () override;
  void AddPendingMove (const Json::Value& mv) override;

public:

  PendingMoves ()
    : pending(Json::objectValue)
  {}

  Json::Value ToJson () const override;

};

} // namespace mover

#endif // MOVER_PENDING_HPP
