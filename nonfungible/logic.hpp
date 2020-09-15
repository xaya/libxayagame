// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_LOGIC_HPP
#define NONFUNGIBLE_LOGIC_HPP

#include <xayagame/sqlitegame.hpp>
#include <xayagame/sqlitestorage.hpp>

#include <json/json.h>

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

  NonFungibleLogic () = default;

  NonFungibleLogic (const NonFungibleLogic&) = delete;
  void operator= (const NonFungibleLogic&) = delete;

};

} // namespace nf

#endif // NONFUNGIBLE_LOGIC_HPP
