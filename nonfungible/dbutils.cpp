// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbutils.hpp"

#include <glog/logging.h>

namespace nf
{

template <>
  std::string
  ColumnExtract<std::string> (sqlite3_stmt* stmt, const int num)
{
  const int len = sqlite3_column_bytes (stmt, num);
  const unsigned char* str = sqlite3_column_text (stmt, num);
  CHECK (str != nullptr);
  return std::string (reinterpret_cast<const char*> (str), len);
}

template <>
  int64_t
  ColumnExtract<int64_t> (sqlite3_stmt* stmt, const int num)
{
  return sqlite3_column_int64 (stmt, num);
}

bool
ColumnIsNull (sqlite3_stmt* stmt, const int num)
{
  return sqlite3_column_type (stmt, num) == SQLITE_NULL;
}

} // namespace nf
