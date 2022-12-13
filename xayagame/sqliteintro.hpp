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

/**
 * Writes a deterministic "description" of the content of the given
 * table onto the output stream.  This will contain the table's schema
 * (from sqlite_master) as well as all content sorted by primary key.
 *
 * The stream must support writing of strings, but nothing else is required.
 */
template <typename Out>
  Out& WriteTableContent (Out& s, const SQLiteDatabase& db,
                          const std::string& table);

/**
 * Writes a deterministic representation of all tables in the given
 * database (with the same behaviour as GetSqliteTables including "internal")
 * to the output stream.
 */
template <typename Out>
  Out& WriteAllTables (Out& s, const SQLiteDatabase& db, bool internal = false);

} // namespace xaya

#include "sqliteintro.tpp"

#endif // XAYAGAME_SQLITEINTRO_HPP
