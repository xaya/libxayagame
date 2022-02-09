// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_LOGIC_HPP
#define XAYASHIPS_LOGIC_HPP

#include "board.hpp"

#include <gamechannel/boardrules.hpp>
#include <gamechannel/channelgame.hpp>
#include <gamechannel/ethsignatures.hpp>
#include <gamechannel/proto/metadata.pb.h>
#include <xayagame/sqlitestorage.hpp>
#include <xayautil/uint256.hpp>

#include <eth-utils/ecdsa.hpp>

#include <json/json.h>

#include <string>

namespace ships
{

/**
 * The number of blocks until a dispute "expires" and force-closes the channel.
 */
constexpr unsigned DISPUTE_BLOCKS = 10;

/**
 * The number of blocks until a channel that has not been joined by a second
 * participant is auto-closed again.
 */
constexpr unsigned CHANNEL_TIMEOUT_BLOCKS = 12;

/**
 * The main game logic for the on-chain part of Xayaships.  This takes care of
 * the public game state (win/loss statistics for names), management of open
 * channels and dispute processing.
 */
class ShipsLogic : public xaya::ChannelGame
{

private:

  const ethutils::ECDSA ctx;
  const xaya::EthSignatureVerifier verifier;

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

  const xaya::SignatureVerifier&
  GetSignatureVerifier () override
  {
    return verifier;
  }

public:

  ShipsLogic ()
    : verifier(ctx)
  {}

  const xaya::BoardRules& GetBoardRules () const override;

};

/**
 * PendingMoveProcessor for Xayaships.  This passes StateProofs recovered
 * from pending disputes and resolutions to ChannelGame::PendingMoves, and
 * keeps track of basic things like created/joined/aborted channels.
 */
class ShipsPending : public xaya::ChannelGame::PendingMoves
{

private:

  /** Pending "create channel" moves, already formatted as JSON.  */
  Json::Value create;

  /**
   * Pending "join channel" moves, already formatted as JSON.  If there
   * are multiple joins for the same channel, we simply return all of them
   * in a JSON array, as the order in which they would be processed in a
   * block is not known beforehand.
   */
  Json::Value join;

  /** Channels being aborted with pending moves.  */
  std::set<xaya::uint256> abort;

  /**
   * Clears the internal state for ships (not including the Clear
   * method for PendingMoves).
   */
  void ClearShips ();

  /**
   * Tries to process a pending "create channel" move.
   */
  void HandleCreateChannel (const Json::Value& obj, const std::string& name,
                            const xaya::uint256& id);

  /**
   * Tries to process a pending "join channel" move.
   */
  void HandleJoinChannel (xaya::SQLiteDatabase& db, const Json::Value& obj,
                          const std::string& name);

  /**
   * Tries to process a pending "abort channel" move.
   */
  void HandleAbortChannel (xaya::SQLiteDatabase& db, const Json::Value& obj,
                           const std::string& name);

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

  void Clear () override;
  void AddPendingMove (const Json::Value& mv) override;

public:

  ShipsPending (ShipsLogic& g)
    : PendingMoves(g)
  {
    ClearShips ();
  }

  Json::Value ToJson () const override;

};

} // namespace ships

#endif // XAYASHIPS_LOGIC_HPP
