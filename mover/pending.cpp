// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pending.hpp"

#include "moves.hpp"
#include "proto/mover.pb.h"

#include <glog/logging.h>

namespace mover
{

void
PendingMoves::Clear ()
{
  pending = Json::Value (Json::objectValue);
}

Json::Value
PendingMoves::ToJson () const
{
  return pending;
}

void
PendingMoves::AddPendingMoveInternal (const xaya::GameStateData& stateStr,
                                           const Json::Value& mv)
{
  const std::string name = mv["name"].asString ();

  proto::Direction dir;
  unsigned steps;
  if (!ParseMove (mv["move"], dir, steps))
    {
      LOG (WARNING) << "Invalid pending move: " << mv;
      return;
    }

  /* Query the game state and find the current player in it.  We need that
     to get the current position, so that we can compute the estimated movement
     target.  (This is not really that useful in practice, but at least it
     allows us to test state-dependent processing of pending moves.)  */
  proto::GameState state;
  CHECK (state.ParseFromString (stateStr));
  int x, y;
  const auto mit = state.players ().find (name);
  if (mit == state.players ().end ())
    {
      x = 0;
      y = 0;
    }
  else
    {
      x = mit->second.x ();
      y = mit->second.y ();
    }

  int dx, dy;
  GetDirectionOffset (dir, dx, dy);
  Json::Value target(Json::objectValue);
  target["x"] = x + static_cast<int> (steps) * dx;
  target["y"] = y + static_cast<int> (steps) * dy;

  Json::Value playerState(Json::objectValue);
  playerState["dir"] = DirectionToString (dir);
  playerState["steps"] = static_cast<int> (steps);
  playerState["target"] = target;

  pending[name] = playerState;
}

void
PendingMoves::AddPendingMove (const Json::Value& mv)
{
  AddPendingMoveInternal (GetConfirmedState (), mv);
}

} // namespace mover
