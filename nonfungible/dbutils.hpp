// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_DBUTILS_HPP
#define NONFUNGIBLE_DBUTILS_HPP

#include <sqlite3.h>

#include <cstdint>
#include <string>

namespace nf
{

/**
 * Binds a value (of string or integer type) to an SQLite parameter.
 */
template <typename T>
  void BindParam (sqlite3_stmt* stmt, int num, const T& val);

/**
 * Binds a null value to an SQLite parameter.
 */
void BindNullParam (sqlite3_stmt* stmt, int num);

/**
 * Extracts a string or integer value from a result column.
 */
template <typename T>
  T ColumnExtract (sqlite3_stmt* stmt, int num);

/**
 * Checks if a result column is null.
 */
bool ColumnIsNull (sqlite3_stmt* stmt, int num);

} // namespace nf

#endif // NONFUNGIBLE_DBUTILS_HPP
