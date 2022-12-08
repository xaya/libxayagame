// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Template implementation code for sqliteintro.hpp.  */

#include <glog/logging.h>

#include <cstdio>

namespace xaya
{

namespace internal
{

/**
 * Queries for all content of the given database table, sorted by primary key,
 * and returns the resulting Statement that can be stepped.
 */
SQLiteDatabase::Statement QueryAllRows (const SQLiteDatabase& db,
                                        const std::string& table);

/**
 * Produces a deterministic representation of the current row's data
 * for the given database statement.  The result is appended onto
 * the given string.
 */
void TableRowContent (std::string& out, const SQLiteDatabase::Statement& stmt);

} // namespace internal

template <typename Out>
  Out&
  WriteTableContent (Out& s, const SQLiteDatabase& db,
                     const std::string& table)
{
  auto stmt = db.PrepareRo (R"(
    SELECT `sql`
      FROM `sqlite_master`
      WHERE `name` = ?1 AND `type` = 'table'
  )");
  stmt.Bind (1, table);

  CHECK (stmt.Step ()) << "No table '" << table << "' exists";
  s << stmt.Get<std::string> (0) << "\n";
  CHECK (!stmt.Step ());

  char numBuf[64];
  std::string row;

  stmt = internal::QueryAllRows (db, table);
  for (unsigned cnt = 0; stmt.Step (); ++cnt)
    {
      std::snprintf (numBuf, sizeof (numBuf), "%d", cnt);
      s << "\nRow " << numBuf << ":\n";

      internal::TableRowContent (row, stmt);
      s << row;
      row.clear ();
    }

  return s;
}

template <typename Out>
  Out&
  WriteAllTables (Out& s, const SQLiteDatabase& db, const bool internal)
{
  const auto tables = GetSqliteTables (db, internal);

  bool first = true;
  for (const auto& t : tables)
    {
      if (!first)
        s << "\n";
      first = false;
      WriteTableContent (s, db, t);
    }

  return s;
}

} // namespace xaya
