// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_LOGIC_HPP
#define XAYASHIPS_LOGIC_HPP

#include "board.hpp"

#include <gamechannel/boardrules.hpp>
#include <gamechannel/channelgame.hpp>
#include <gamechannel/proto/metadata.pb.h>
#include <xayagame/sqlitestorage.hpp>
#include <xayautil/uint256.hpp>

#include <json/json.h>

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
   * Tries to process a move declaring one participant of a channel
   * the loser.
   */
  void HandleDeclareLoss (xaya::SQLiteDatabase& db,
                          const Json::Value& obj, const std::string& name);

  /**
   * Tries to process a dispute/resolution move.
   */
  void HandleDisputeResolution (xaya::SQLiteDatabase& db,
                                const Json::Value& obj, unsigned height,
                                bool isDispute);

  /**
   * Processes all expired disputes, force-closing the channels.
   */
  void ProcessExpiredDisputes (xaya::SQLiteDatabase& db, unsigned height);

  /**
   * Updates the game stats in the global database state for a channel that
   * is being closed with the given winner.  Note that this does not close
   * (remove) the channel itself from the database; it just updates the
   * game_stats table.
   */
  static void UpdateStats (xaya::SQLiteDatabase& db,
                           const xaya::proto::ChannelMetadata& meta,
                           int winner);

  friend class InMemoryLogicFixture;
  friend class StateUpdateTests;
  friend class SchemaTests;

protected:

  void SetupSchema (xaya::SQLiteDatabase& db) override;

  void GetInitialStateBlock (unsigned& height,
                             std::string& hashHex) const override;
  void InitialiseState (xaya::SQLiteDatabase& db) override;

  void UpdateState (xaya::SQLiteDatabase& db,
                    const Json::Value& blockData) override;

  Json::Value GetStateAsJson (const xaya::SQLiteDatabase& db) override;

public:

  const xaya::BoardRules& GetBoardRules () const override;

};

/**
 * PendingMoveProcessor for Xayaships.  This just passes StateProofs recovered
 * from pending disputes and resolutions to ChannelGame::PendingMoves.
 */
class ShipsPending : public xaya::ChannelGame::PendingMoves
{

private:

  /**
   * Tries to process a pending dispute or resolution move.
   */
  void HandleDisputeResolution (xaya::SQLiteDatabase& db,
                                const Json::Value& obj);

  /**
   * Processes a new move, but does not call AccessConfirmedState.  This is
   * used in tests, so that we can get away without setting up a consistent
   * current state in the database.
   */
  void AddPendingMoveUnsafe (const xaya::SQLiteDatabase& db,
                             const Json::Value& mv);

  friend class PendingTests;

protected:

  void AddPendingMove (const Json::Value& mv) override;

public:

  ShipsPending (ShipsLogic& g)
    : PendingMoves(g)
  {}

};

} // namespace ships

#endif // XAYASHIPS_LOGIC_HPP
