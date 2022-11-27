// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqliteintro.hpp"

#include <glog/logging.h>

namespace xaya
{

namespace
{

/**
 * Returns true if the given table name should be considered "internal".
 */
bool
IsInternalTable (const std::string& nm)
{
  for (const std::string prefix : {"xayagame_", "sqlite_"})
    if (nm.substr (0, prefix.size ()) == prefix)
      return true;

  return false;
}

} // anonymous namespace

std::set<std::string>
GetSqliteTables (const SQLiteDatabase& db, const bool internal)
{
  auto stmt = db.PrepareRo (R"(
    SELECT `name`
      FROM `sqlite_master`
      WHERE `type` = 'table'
  )");

  std::set<std::string> res;
  while (stmt.Step ())
    {
      const auto name = stmt.Get<std::string> (0);
      if (internal || !IsInternalTable (name))
        res.insert (name);
    }

  return res;
}

std::set<std::string>
GetTableColumns (const SQLiteDatabase& db, const std::string& table)
{
  auto stmt = db.PrepareRo ("SELECT * FROM `" + table + "` LIMIT 0");
  /* No need to step, we just want the metadata of the prepared statement,
     which is already available.  */

  std::set<std::string> res;
  const int cnt = sqlite3_column_count (stmt.ro ());
  for (int i = 0; i < cnt; ++i)
    res.insert (sqlite3_column_name (stmt.ro (), i));

  return res;
}

std::set<std::string>
GetPrimaryKeyColumns (const SQLiteDatabase& db, const std::string& table,
                      const std::set<std::string>& columns)
{
  std::set<std::string> res;
  db.ReadDatabase ([&] (sqlite3* rawDb)
    {
      for (const auto& c : columns)
        {
          int isPk;
          CHECK_EQ (sqlite3_table_column_metadata (
                        rawDb, nullptr, table.c_str (), c.c_str (),
                        nullptr, nullptr, nullptr, &isPk, nullptr),
                    SQLITE_OK);
          if (isPk)
            res.insert (c);
        }
    });

  return res;
}

} // namespace xaya
