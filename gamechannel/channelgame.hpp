// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELGAME_HPP
#define GAMECHANNEL_CHANNELGAME_HPP

#include "boardrules.hpp"
#include "database.hpp"

#include "proto/stateproof.pb.h"

#include <xayagame/sqlitegame.hpp>

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
   * state or at least the same as the current state and it does not have
   * a dispute yet), then the dispute is opened on the ChannelData instance and
   * true is returned.  If it is not valid, then no changes are made and
   * false is returned.
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
  friend class ChannelsTable;

public:

  using SQLiteGame::SQLiteGame;

};

} // namespace xaya

#endif // GAMECHANNEL_CHANNELGAME_HPP
