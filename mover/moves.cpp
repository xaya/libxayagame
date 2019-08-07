// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "moves.hpp"

#include <glog/logging.h>

namespace mover
{

void
GetDirectionOffset (const proto::Direction dir, int& dx, int& dy)
{
  switch (dir)
    {
    case proto::RIGHT:
      dx = 1;
      dy = 0;
      return;

    case proto::LEFT:
      dx = -1;
      dy = 0;
      return;

    case proto::UP:
      dx = 0;
      dy = 1;
      return;

    case proto::DOWN:
      dx = 0;
      dy = -1;
      return;

    case proto::RIGHT_UP:
      dx = 1;
      dy = 1;
      return;

    case proto::RIGHT_DOWN:
      dx = 1;
      dy = -1;
      return;

    case proto::LEFT_UP:
      dx = -1;
      dy = 1;
      return;

    case proto::LEFT_DOWN:
      dx = -1;
      dy = -1;
      return;

    default:
      LOG (FATAL) << "Unexpected direction: " << dir;
      return;
    }
}

std::string
DirectionToString (const proto::Direction dir)
{
  switch (dir)
    {
    case proto::NONE:
      return "none";
    case proto::RIGHT:
      return "right";
    case proto::LEFT:
      return "left";
    case proto::UP:
      return "up";
    case proto::DOWN:
      return "down";
    case proto::RIGHT_UP:
      return "right-up";
    case proto::RIGHT_DOWN:
      return "right-down";
    case proto::LEFT_UP:
      return "left-up";
    case proto::LEFT_DOWN:
      return "left-down";
    }
  LOG (FATAL) << "Unexpected direction: " << dir;
}

namespace
{

/**
 * Converts a direction string to the proto enum direction.  Returns NONE
 * if it is not a valid string.
 */
proto::Direction
ParseDirection (const std::string& str)
{
  if (str == "l")
    return proto::RIGHT;
  if (str == "h")
    return proto::LEFT;
  if (str == "k")
    return proto::UP;
  if (str == "j")
    return proto::DOWN;
  if (str == "u")
    return proto::RIGHT_UP;
  if (str == "n")
    return proto::RIGHT_DOWN;
  if (str == "y")
    return proto::LEFT_UP;
  if (str == "b")
    return proto::LEFT_DOWN;

  return proto::NONE;
}

} // anonymous namespace

bool
ParseMove (const Json::Value& obj, proto::Direction& dir, unsigned& steps)
{
  if (!obj.isObject ())
    return false;
  if (obj.size () != 2)
    return false;

  if (!obj.isMember ("d") || !obj.isMember ("n"))
    return false;

  const Json::Value& d = obj["d"];
  const Json::Value& n = obj["n"];
  if (!d.isString () || !n.isUInt ())
    return false;

  dir = ParseDirection (d.asString ());
  if (dir == proto::NONE)
    return false;
  steps = n.asUInt ();
  if (steps <= 0 || steps > 1000000)
    return false;

  return true;
}

} // namespace mover
