// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_GAMESTATEJSON_HPP
#define XAYASHIPS_GAMESTATEJSON_HPP

#include "board.hpp"

#include <xayagame/sqlitestorage.hpp>

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

  /** The underlying database instance.  */
  const xaya::SQLiteDatabase& db;

  /** Our board rules.  */
  const ShipsBoardRules& rules;

public:

  GameStateJson (const xaya::SQLiteDatabase& d, const ShipsBoardRules& r)
    : db(d), rules(r)
  {}

  GameStateJson () = delete;
  GameStateJson (const GameStateJson&) = delete;
  void operator= (const GameStateJson&) = delete;

  /**
   * Extracts the full current state as JSON.
   */
  Json::Value GetFullJson () const;

};

} // namespace ships

#endif // XAYASHIPS_GAMESTATEJSON_HPP
