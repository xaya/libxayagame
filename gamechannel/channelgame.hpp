// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELGAME_HPP
#define GAMECHANNEL_CHANNELGAME_HPP

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

public:

  using SQLiteGame::SQLiteGame;

};

} // namespace xaya

#endif // GAMECHANNEL_CHANNELGAME_HPP
