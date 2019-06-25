// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_GAMESTATEJSON_HPP
#define XAYASHIPS_GAMESTATEJSON_HPP

#include "logic.hpp"

#include <json/json.h>

namespace ships
{

/**
 * Helper class that allows extracting game-state data as JSON from the
 * current Xayaships global state.
 */
class GameStateJson
{

private:

  /** The underlying Xayaships logic instance.  */
  ShipsLogic& rules;

public:

  GameStateJson (ShipsLogic& r)
    : rules(r)
  {}

  GameStateJson () = delete;
  GameStateJson (const GameStateJson&) = delete;
  void operator= (const GameStateJson&) = delete;

  /**
   * Extracts the full current state as JSON.
   */
  Json::Value GetFullJson ();

};

} // namespace ships

#endif // XAYASHIPS_GAMESTATEJSON_HPP
