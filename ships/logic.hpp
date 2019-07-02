// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_LOGIC_HPP
#define XAYASHIPS_LOGIC_HPP

#include "board.hpp"

#include <gamechannel/boardrules.hpp>
#include <gamechannel/channelgame.hpp>
#include <gamechannel/proto/metadata.pb.h>
#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <sqlite3.h>

#include <string>

namespace ships
{

/**
 * The number of blocks until a dispute "expires" and force-closes the channel.
 */
constexpr unsigned DISPUTE_BLOCKS = 10;

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
  void HandleJoinChannel (const Json::Value& obj, const std::string& name,
                          const xaya::uint256& txid);

  /**
   * Tries to process an "abort channel" move.
   */
  void HandleAbortChannel (const Json::Value& obj, const std::string& name);

  /**
   * Tries to process a channel close with winner statement.
   */
  void HandleCloseChannel (const Json::Value& obj);

  /**
   * Tries to process a dispute/resolution move.
   */
  void HandleDisputeResolution (const Json::Value& obj, unsigned height,
                                bool isDispute);

  /**
   * Processes all expired disputes, force-closing the channels.
   */
  void ProcessExpiredDisputes (unsigned height);

  /**
   * Updates the game stats in the global database state for a channel that
   * is being closed with the given winner.  Note that this does not close
   * (remove) the channel itself from the database; it just updates the
   * game_stats table.
   */
  void UpdateStats (const xaya::proto::ChannelMetadata& meta, int winner);

  /**
   * Binds a TEXT SQLite parameter to a string.  This is a utility method that
   * is also used for tests, and thus exposed here.
   */
  static void BindStringParam (sqlite3_stmt* stmt, int ind,
                               const std::string& str);

  /* This class is not test-code, but it is basically a part of the
     implementation of ShipsLogic itself that is just moved out to a separate
     file / compilation unit to make the code easier to read.  */
  friend class GameStateJson;

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
