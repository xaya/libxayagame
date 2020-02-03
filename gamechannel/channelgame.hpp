// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELGAME_HPP
#define GAMECHANNEL_CHANNELGAME_HPP

#include "boardrules.hpp"
#include "database.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayagame/sqlitegame.hpp>
#include <xayautil/uint256.hpp>

namespace xaya
{

/**
 * Games using game channels should base their core on-chain game daemon on
 * this class.  It leaves it up to concrete implementations to fill in the
 * callbacks for SQLiteGame, but it provides some functions for general
 * handling of game-channel operations that can be utilised from the game's
 * move-processing callbacks.
 */
class ChannelGame : public SQLiteGame
{

protected:

  /**
   * Sets up the game-channel-related database schema.  This method should be
   * called from the overridden SetupSchema method.
   */
  void SetupGameChannelsSchema (SQLiteDatabase& db);

  /**
   * Processes a request (e.g. sent in a move) to open a dispute at the
   * current block height for the given game channel and based on the
   * given state proof.  If the request is valid (mainly meaning that the
   * state proof is valid and for a "later" state than the current on-chain
   * state), then the dispute is opened on the ChannelData instance and
   * true is returned.  If it is not valid, then no changes are made and
   * false is returned.
   *
   * It is valid to open a dispute for the state that is currently on-chain
   * (same turn height but only if it actually Equals() that state) if there
   * was not already a dispute for it.  This is necessary to avoid a situation
   * as in https://github.com/xaya/libxayagame/issues/51.
   */
  bool ProcessDispute (ChannelData& ch, unsigned height,
                       const proto::StateProof& proof);

  /**
   * Processes a request (e.g. sent in a move) for resolving a dispute
   * in the given channel.  If the provided state proof is valid and at least
   * one turn further than the current on-chain state, then the new state is
   * put on-chain and any open disputes are resolved (and true is returned).
   * Note that this function succeeds also if there is not an open dispute;
   * in that case, the on-chain state will simply be updated.
   */
  bool ProcessResolution (ChannelData& ch, const proto::StateProof& proof);

  /**
   * This method needs to be overridden to provide an instance of BoardRules
   * to the game channels framework.
   */
  virtual const BoardRules& GetBoardRules () const = 0;

  friend class ChannelData;
  friend class ChannelGspRpcServer;
  friend class ChannelsTable;

public:

  class PendingMoves;

  using SQLiteGame::SQLiteGame;

};

/**
 * PendingMoveProcessor for a channel game's GSP.  This has functionality
 * to build up "standard pending data", which contains state proofs known
 * from pending disputes and resolutions.
 *
 * Subclasses must still implement AddPendingMove by themselves, where they
 * at least have to parse their game-specific move format and pass any received
 * state proofs (e.g. disputes and resolutions) on to AddPendingStateProof
 * for processing by this class.
 */
class ChannelGame::PendingMoves : public SQLiteGame::PendingMoves
{

private:

  /**
   * The data stored for the pending state proof of one of the channels.
   */
  struct PendingChannelData
  {

    /** The actual StateProof corresponding to the latest known state.  */
    proto::StateProof proof;

    /** The turn count of this state proof.  */
    unsigned turnCount;

  };

  /** Data for all channels that have pending updates.  */
  std::map<uint256, PendingChannelData> channels;

protected:

  /**
   * Clears the current state.  In case subclasses override Clear themselves,
   * they should make sure to also call this version.
   */
  void Clear () override;

  /**
   * Processes a new StateProof received in a pending move for the given
   * channel.  This verifies that the StateProof is valid and later than
   * what we may have already, but it does not verify other conditions that
   * may be imposed by ProcessDispute or ProcessResolution.  In other words,
   * it may be that an update makes it into the pending state even though
   * the corresponding move will be invalid when processed on-chain.  That is
   * not an issue, though, as every fresher update than previously known
   * can be useful for the channel game (independent of what the move
   * will do in the end).
   */
  void AddPendingStateProof (ChannelData& ch, const proto::StateProof& proof);

public:

  explicit PendingMoves (ChannelGame& g)
    : SQLiteGame::PendingMoves(g)
  {}

  /**
   * Returns the pending channel state as JSON.  The JSON result will be
   * an object with per-channel pending data in a "channels" field.
   *
   * If subclasses want to return more data, they should call this method
   * and then extend the resulting JSON object with more fields.  They should
   * not change the structure or remove fields, since that would break the
   * general ChainToChannel logic reading this data.
   */
  Json::Value ToJson () const override;

};

/**
 * Updates the reinitialisation ID in the given metadata proto for an update
 * done by the passed-in txid.  This is one way to update the reinit IDs and
 * make sure that they yield a unique sequence that does not allow for any
 * replay attacks.  It need not be used by games, though, in case they have
 * a more suitable update mechanism.
 */
void UpdateMetadataReinit (const uint256& txid, proto::ChannelMetadata& meta);

} // namespace xaya

#endif // GAMECHANNEL_CHANNELGAME_HPP
