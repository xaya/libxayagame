// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqliteintro.hpp"

#include <xayautil/hash.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sstream>

namespace xaya
{
namespace
{

using testing::ElementsAre;

constexpr const char* TABLE_AUTOINC = R"(
  `key` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  `text` TEXT NULL,
  `int` INTEGER NULL
)";

constexpr const char* TABLE_PK = R"(
  `foo` INTEGER NOT NULL,
  `bar` INTEGER NOT NULL,
  -- Some comment that will be kept in SQL schema.
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
      INSERT INTO `autoinc`
        (`text`, `int`) VALUES
        ('foo', 10), (NULL, NULL), ('bar', 42);
      INSERT INTO `pk`
        (`foo`, `bar`, `blob`) VALUES
        (1, 1, 'abc'),
        -- Some type conversion checks here.  The '123' will be seen as text
        -- still (since the column is declared as such), while the '2' will
        -- be seen as integer 2.
        (2, '2', '123'),
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
  EXPECT_THAT (GetTableColumns (db, "autoinc"),
               ElementsAre ("int", "key", "text"));
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
               ElementsAre ("key"));

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

class SQLiteWriteAllTablesTests : public SQLiteIntroTests
{

protected:

  /**
   * Hashes all DB content with SHA256.
   */
  uint256
  HashDb ()
  {
    SHA256 hasher;
    WriteAllTables (hasher, db);
    return hasher.Finalise ();
  }

};

TEST_F (SQLiteWriteAllTablesTests, GoldenData)
{
  std::ostringstream out;
  WriteAllTables (out, db);

  std::ostringstream expected;

  expected << "CREATE TABLE `autoinc` (" << TABLE_AUTOINC << ")\n";
  expected << "\nRow 0:\n"
           << "  key: INTEGER 1\n"
           << "  text: DATA-SHA256 " << SHA256::Hash ("foo").ToHex () << "\n"
           << "  int: INTEGER 10\n";
  expected << "\nRow 1:\n"
           << "  key: INTEGER 2\n"
           << "  text: NULL\n"
           << "  int: NULL\n";
  expected << "\nRow 2:\n"
           << "  key: INTEGER 3\n"
           << "  text: DATA-SHA256 " << SHA256::Hash ("bar").ToHex () << "\n"
           << "  int: INTEGER 42\n";

  expected << "\nCREATE TABLE `empty` (" << TABLE_EMPTY << ")\n";

  expected << "\nCREATE TABLE `pk` (" << TABLE_PK << ")\n";
  expected << "\nRow 0:\n"
           << "  foo: INTEGER 0\n"
           << "  bar: INTEGER 0\n"
           << "  blob: DATA-SHA256 "
           << SHA256::Hash (std::string ("x\0y", 3)).ToHex () << "\n";
  expected << "\nRow 1:\n"
           << "  foo: INTEGER 1\n"
           << "  bar: INTEGER 1\n"
           << "  blob: DATA-SHA256 " << SHA256::Hash ("abc").ToHex () << "\n";
  expected << "\nRow 2:\n"
           << "  foo: INTEGER 2\n"
           << "  bar: INTEGER 1\n"
           << "  blob: DATA-SHA256 " << SHA256::Hash ("").ToHex () << "\n";
  expected << "\nRow 3:\n"
           << "  foo: INTEGER 1\n"
           << "  bar: INTEGER 2\n"
           << "  blob: NULL\n";
  expected << "\nRow 4:\n"
           << "  foo: INTEGER 2\n"
           << "  bar: INTEGER 2\n"
           << "  blob: DATA-SHA256 " << SHA256::Hash ("123").ToHex () << "\n";

  EXPECT_EQ (out.str (), expected.str ());
}

TEST_F (SQLiteWriteAllTablesTests, ExplicitOrdering)
{
  const auto hash1 = HashDb ();
  db.Execute ("PRAGMA `reverse_unordered_selects` = 1");
  const auto hash2 = HashDb ();
  db.Execute ("PRAGMA `reverse_unordered_selects` = 0");
  const auto hash3 = HashDb ();

  EXPECT_EQ (hash2, hash1);
  EXPECT_EQ (hash3, hash1);
}

TEST_F (SQLiteWriteAllTablesTests, DetectsChange)
{
  const auto hash1 = HashDb ();

  db.Execute (R"(
    UPDATE `autoinc`
      SET `int` = 'x'
      WHERE `int` = 42
  )");
  const auto hash2 = HashDb ();

  db.Execute (R"(
    UPDATE `autoinc`
      SET `int` = 42
      WHERE `int` = 'x'
  )");
  const auto hash3 = HashDb ();

  EXPECT_NE (hash2, hash1);
  EXPECT_EQ (hash3, hash1);
}

} // anonymous namespace
} // namespace xaya
