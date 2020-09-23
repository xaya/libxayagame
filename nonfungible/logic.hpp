// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_LOGIC_HPP
#define NONFUNGIBLE_LOGIC_HPP

#include "statejson.hpp"

#include <xayagame/sqlitegame.hpp>
#include <xayagame/sqlitestorage.hpp>

#include <json/json.h>

#include <functional>
#include <string>

namespace nf
{

/**
 * The game logic implementation for the non-fungible game-state processor.
 */
class NonFungibleLogic : public xaya::SQLiteGame
{

protected:

  void SetupSchema (xaya::SQLiteDatabase& db) override;

  void GetInitialStateBlock (unsigned& height,
                             std::string& hashHex) const override;
  void InitialiseState (xaya::SQLiteDatabase& db) override;

  void UpdateState (xaya::SQLiteDatabase& db,
                    const Json::Value& blockData) override;

  Json::Value GetStateAsJson (const xaya::SQLiteDatabase& db) override;

public:

  /**
   * Type for a callback that extracts custom JSON from the game state
   * (through a StateJsonExtractor instance).
   */
  using StateCallback
      = std::function<Json::Value (const StateJsonExtractor& ext)>;

  NonFungibleLogic () = default;

  NonFungibleLogic (const NonFungibleLogic&) = delete;
  void operator= (const NonFungibleLogic&) = delete;

  /**
   * Extracts some custom JSON from the current game-state database, using
   * the provided extractor callback, which can then operate through a
   * StateJsonExtractor instance.
   */
  Json::Value GetCustomStateData (xaya::Game& game, const StateCallback& cb);

};

} // namespace nf

#endif // NONFUNGIBLE_LOGIC_HPP
