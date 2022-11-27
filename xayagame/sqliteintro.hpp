// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_SQLITEINTRO_HPP
#define XAYAGAME_SQLITEINTRO_HPP

#include "sqlitestorage.hpp"

#include <set>
#include <string>

namespace xaya
{

/**
 * Returns a list of all tables in the given SQLite database.  If internal
 * is false, then xayagame_* and sqlite_* tables are filtered out.
 */
std::set<std::string> GetSqliteTables (const SQLiteDatabase& db,
                                       bool internal = false);

/**
 * Lists all column names of the given table in the SQLite database.
 */
std::set<std::string> GetTableColumns (const SQLiteDatabase& db,
                                       const std::string& table);

/**
 * Given the list of columns in a table, returns the subset of columns
 * that make up the primary key.
 */
std::set<std::string> GetPrimaryKeyColumns (
    const SQLiteDatabase& db, const std::string& table,
    const std::set<std::string>& columns);

} // namespace xaya

#endif // XAYAGAME_SQLITEINTRO_HPP
