// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_LOGIC_HPP
#define XAYASHIPS_LOGIC_HPP

#include "board.hpp"

#include <gamechannel/boardrules.hpp>
#include <gamechannel/channelgame.hpp>
#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <sqlite3.h>

#include <string>

namespace ships
{

/**
 * The main game logic for the on-chain part of Xayaships.  This takes care of
 * the public game state (win/loss statistics for names), management of open
 * channels and dispute processing.
 */
class ShipsLogic : public xaya::ChannelGame
{

private:

  ShipsBoardRules boardRules;

  /**
   * Tries to process a "create channel" move, if the JSON object describes
   * a valid one.
   */
  void HandleCreateChannel (const Json::Value& obj, const std::string& name,
                            const xaya::uint256& txid);

  /**
   * Tries to process a "join channel" move.
   */
  void HandleJoinChannel (const Json::Value& obj, const std::string& name);

  friend class InMemoryLogicFixture;
  friend class StateUpdateTests;
  friend class SchemaTests;

protected:

  const xaya::BoardRules& GetBoardRules () const override;

  void SetupSchema (sqlite3* db) override;

  void GetInitialStateBlock (unsigned& height,
                             std::string& hashHex) const override;
  void InitialiseState (sqlite3* db) override;

  void UpdateState (sqlite3* db, const Json::Value& blockData) override;

  Json::Value GetStateAsJson (sqlite3* db) override;

};

} // namespace ships

#endif // XAYASHIPS_LOGIC_HPP
