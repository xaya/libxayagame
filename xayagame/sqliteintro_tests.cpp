// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqliteintro.hpp"

#include "sqlitestorage.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace xaya
{
namespace
{

using testing::ElementsAre;

constexpr const char* TABLE_AUTOINC = R"(
  `int` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  `text` TEXT NULL
)";

constexpr const char* TABLE_PK = R"(
  `foo` INTEGER NOT NULL,
  `bar` INTEGER NOT NULL,
  `blob` BLOB NULL,
  PRIMARY KEY (`foo`, `bar`)
)";

constexpr const char* TABLE_EMPTY = R"(
  `id` INTEGER NOT NULL,
  PRIMARY KEY (`id`)
)";

class SQLiteIntroTests : public testing::Test
{

protected:

  SQLiteDatabase db;

  SQLiteIntroTests ()
    : db("foo", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY)
  {
    db.Execute (std::string ("CREATE TABLE `autoinc` (") + TABLE_AUTOINC + ")");
    db.Execute (std::string ("CREATE TABLE `pk` (") + TABLE_PK + ")");
    db.Execute (std::string ("CREATE TABLE `empty` (") + TABLE_EMPTY + ")");
    db.Execute (R"(
      INSERT INTO `autoinc` (`text`) VALUES ('foo'), (NULL), ('bar');
      INSERT INTO `pk`
        (`foo`, `bar`, `blob`) VALUES
        (1, 1, 'abc'),
        (2, 2, 'xyz'),
        (1, 2, NULL),
        (2, 1, '');

      -- Just some random index to make sure it is not seen as table.
      CREATE INDEX `pk_foo` ON `pk` (`blob`, `foo`);
    )");

    /* Insert a real "blob" with nul character.  */
    auto stmt = db.Prepare (R"(
      INSERT INTO `pk` (`foo`, `bar`, `blob`) VALUES (0, 0, ?1)
    )");
    stmt.Bind (1, std::string ("x\0y", 3));
    stmt.Execute ();
  }

};

TEST_F (SQLiteIntroTests, GetSqliteTables)
{
  db.Execute (R"(
    CREATE TABLE `xayagame_dummy` (`foo` INTEGER NOT NULL PRIMARY KEY);
  )");

  EXPECT_THAT (GetSqliteTables (db), ElementsAre ("autoinc", "empty", "pk"));
  EXPECT_THAT (GetSqliteTables (db, true),
               ElementsAre ("autoinc", "empty", "pk",
                            "sqlite_sequence", "xayagame_dummy"));
}

TEST_F (SQLiteIntroTests, GetTableColumns)
{
  EXPECT_THAT (GetTableColumns (db, "autoinc"), ElementsAre ("int", "text"));
  EXPECT_THAT (GetTableColumns (db, "pk"), ElementsAre ("bar", "blob", "foo"));
  EXPECT_THAT (GetTableColumns (db, "empty"), ElementsAre ("id"));
}

TEST_F (SQLiteIntroTests, GetPrimaryKeyColumns)
{
  db.Execute (R"(
    CREATE TABLE `no_pk` (`something` TEXT NULL);
  )");

  std::set<std::string> columns;

  columns = GetTableColumns (db, "autoinc");
  EXPECT_THAT (GetPrimaryKeyColumns (db, "autoinc", columns),
               ElementsAre ("int"));

  columns = GetTableColumns (db, "pk");
  EXPECT_THAT (GetPrimaryKeyColumns (db, "pk", columns),
               ElementsAre ("bar", "foo"));

  columns = GetTableColumns (db, "empty");
  EXPECT_THAT (GetPrimaryKeyColumns (db, "empty", columns),
               ElementsAre ("id"));

  columns = GetTableColumns (db, "no_pk");
  EXPECT_THAT (GetPrimaryKeyColumns (db, "no_pk", columns),
               ElementsAre ());
}

} // anonymous namespace
} // namespace xaya
