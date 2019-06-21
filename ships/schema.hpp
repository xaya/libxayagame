// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_SCHEMA_HPP
#define XAYASHIPS_SCHEMA_HPP

#include <sqlite3.h>

namespace ships
{

/**
 * Sets up or updates the database schema for the on-chain state of
 * Xayaships, not including data of the game channels themselves (which
 * is managed by the game-channel framework).
 */
void SetupShipsSchema (sqlite3* db);

} // namespace ships

#endif // XAYASHIPS_SCHEMA_HPP
