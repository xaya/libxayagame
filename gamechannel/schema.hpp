// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_SCHEMA_HPP
#define GAMECHANNEL_SCHEMA_HPP

/* This file is an implementation detail of the game-channels framework
   and should not be used directly by external code!  */

#include "xayagame/sqlitestorage.hpp"

namespace xaya
{

/**
 * Sets up or updates the database schema for the internal representation
 * of game channels in the on-chain game state.
 */
void InternalSetupGameChannelsSchema (SQLiteDatabase& db);

} // namespace xaya

#endif // GAMECHANNEL_SCHEMA_HPP
