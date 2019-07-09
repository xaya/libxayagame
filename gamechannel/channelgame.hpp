// Copyright (C) 2019 The Xaya developers
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
  void SetupGameChannelsSchema (sqlite3* db);

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

  using SQLiteGame::SQLiteGame;

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
