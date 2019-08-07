// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MOVER_MOVES_HPP
#define MOVER_MOVES_HPP

#include "proto/mover.pb.h"

#include <json/json.h>

#include <string>

namespace mover
{

/**
 * Converts a movement direction to a string for the JSON state.
 */
std::string DirectionToString (proto::Direction dir);

/**
 * Returns the offset in x and y coordinates for a step in the given direction.
 */
void GetDirectionOffset (proto::Direction dir, int& dx, int& dy);

/**
 * Parses a move object into direction and number of steps.  Returns false
 * if the move is somehow invalid.
 */
bool ParseMove (const Json::Value& obj, proto::Direction& dir, unsigned& steps);

} // namespace mover

#endif // MOVER_MOVES_HPP
