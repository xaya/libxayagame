// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqliteintro.hpp"

#include <xayautil/hash.hpp>

#include <glog/logging.h>

#include <cinttypes>
#include <sstream>

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

namespace internal
{

SQLiteDatabase::Statement
QueryAllRows (const SQLiteDatabase& db, const std::string& table)
{
  const auto columns = GetTableColumns (db, table);
  const auto pk = GetPrimaryKeyColumns (db, table, columns);
  CHECK (!pk.empty ()) << "Primary key for table '" << table << "' is empty";

  std::ostringstream pkColumns;
  bool first = true;
  for (const auto& p : pk)
    {
      if (!first)
        pkColumns << ", ";
      first = false;
      pkColumns << "`" << p << "`";
    }

  std::ostringstream sql;
  sql << "SELECT * FROM `" << table << "` ORDER BY " << pkColumns.str ();

  return db.PrepareRo (sql.str ());
}

void
TableRowContent (std::string& out, const SQLiteDatabase::Statement& stmt)
{
  auto* raw = stmt.ro ();

  char numBuf[64];

  const int cnt = sqlite3_column_count (raw);
  for (int i = 0; i < cnt; ++i)
    {
      out += "  ";
      out += sqlite3_column_name (raw, i);
      out += ": ";
      switch (sqlite3_column_type (raw, i))
        {
        case SQLITE_INTEGER:
          std::snprintf (numBuf, sizeof (numBuf),
                         "%" PRId64, stmt.Get<int64_t> (i));
          out += "INTEGER ";
          out += numBuf;
          break;

        case SQLITE_NULL:
          out += "NULL";
          break;

        case SQLITE_TEXT:
        case SQLITE_BLOB:
          out += "DATA-SHA256 ";
          out += SHA256::Hash (stmt.Get<std::string> (i)).ToHex ();
          break;

        case SQLITE_FLOAT:
          LOG (FATAL) << "Database column must not be FLOAT";
        default:
          LOG (FATAL) << "Unexpected column default type";
        }
      out += "\n";
    }
}

} // namespace internal

} // namespace xaya
