// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_SCHEMA_HPP
#define XAYASHIPS_SCHEMA_HPP

#include "xayagame/sqlitestorage.hpp"

namespace ships
{

/**
 * Sets up or updates the database schema for the on-chain state of
 * Xayaships, not including data of the game channels themselves (which
 * is managed by the game-channel framework).
 */
void SetupShipsSchema (xaya::SQLiteDatabase& db);

} // namespace ships

#endif // XAYASHIPS_SCHEMA_HPP
