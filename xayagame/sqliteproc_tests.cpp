// Copyright (C) 2022-2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqliteproc.hpp"

#include "sqliteintro.hpp"

#include <xayautil/hash.hpp>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <map>
#include <sstream>

namespace xaya
{
namespace
{

/* ************************************************************************** */

/**
 * Expects that the values recorded in a key/value table match the given set.
 * The block hashes are constructed by hashing the key strings.
 */
template <typename T>
  void
  ExpectRecords (const SQLiteDatabase& db,
                 const std::string& table, const std::string& valueCol,
                 const std::map<std::string, T>& expected)
  {
    std::map<uint256, T> expectedHashed;
    for (const auto& entry : expected)
      expectedHashed.emplace (SHA256::Hash (entry.first), entry.second);

    std::ostringstream sql;
    sql << "SELECT `block`, `" << valueCol << "` FROM `" << table << "`";

    std::map<uint256, T> actual;
    auto stmt = db.PrepareRo (sql.str ());
    while (stmt.Step ())
      actual.emplace (stmt.Get<uint256> (0), stmt.Get<T> (1));

    EXPECT_EQ (actual, expectedHashed);
  }

/**
 * A simple processor class, which just records the value from the test database
 * "onerow" table with block hashes (without hashing the state otherwise,
 * so it can be easily compared to known expected values).
 */
class TestProcessor : public SQLiteProcessor
{

private:

  uint256 block;
  std::string value;

protected:

  void
  Compute (const Json::Value& blockData, const SQLiteDatabase& db) override
  {
    CHECK (block.FromHex (blockData["hash"].asString ()));
    auto stmt = db.PrepareRo (R"(
      SELECT `text` FROM `onerow`
    )");
    CHECK (stmt.Step ());
    value = stmt.Get<std::string> (0);
    CHECK (!stmt.Step ());
  }

  void
  Store (SQLiteDatabase& db) override
  {
    auto stmt = db.Prepare (R"(
      INSERT OR IGNORE INTO `xayagame_procvalues`
        (`block`, `value`) VALUES (?1, ?2)
    )");
    stmt.Bind (1, block);
    stmt.Bind (2, value);
    stmt.Execute ();
  }

public:

  void
  SetupSchema (SQLiteDatabase& db) override
  {
    SQLiteProcessor::SetupSchema (db);
    db.Execute (R"(
      CREATE TABLE IF NOT EXISTS `xayagame_procvalues`
          (`block` BLOB PRIMARY KEY,
           `value` TEXT NOT NULL);
    )");
  }

};

class SQLiteProcTests : public testing::Test
{

protected:

  SQLiteDatabase db;
  TestProcessor proc;

  SQLiteProcTests ()
    : db("foo", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY)
  {
    /* We use a single table with a single row and just some value
       as the underlying "game state" database, so that it is easy to
       create / modify / restore for testing.  */
    db.Execute (R"(
      CREATE TABLE `onerow` (`text` TEXT PRIMARY KEY);
      INSERT INTO `onerow` (`text`) VALUES ('initial');
    )");
    proc.SetupSchema (db);
  }

  /**
   * Sets the value in the database.
   */
  void
  SetValue (const std::string& val)
  {
    auto stmt = db.Prepare (R"(
      UPDATE `onerow` SET `text` = ?1
    )");
    stmt.Bind (1, val);
    stmt.Execute ();
  }

  /**
   * Expects that the processor has recorded the given values.
   */
  void
  ExpectValues (const std::map<std::string, std::string>& expected)
  {
    proc.Finish (db);
    ExpectRecords (db, "xayagame_procvalues", "value", expected);
  }

  /**
   * Builds fake JSON block data for the given block height.  The block
   * hash will be the SHA256 of the block string.
   */
  static Json::Value
  BlockData (const unsigned height, const std::string& blk)
  {
    Json::Value blockData(Json::objectValue);
    blockData["height"] = static_cast<Json::Int64> (height);
    blockData["hash"] = SHA256::Hash (blk).ToHex ();
    return blockData;
  }

};

TEST_F (SQLiteProcTests, RunsAtDefinedInterval)
{
  proc.SetInterval (3, 1);

  proc.Process (BlockData (0, "zero"), db, nullptr);
  proc.Process (BlockData (1, "one"), db, nullptr);
  proc.Process (BlockData (2, "two"), db, nullptr);
  SetValue ("changed");
  proc.Process (BlockData (3, "three"), db, nullptr);
  proc.Process (BlockData (4, "four"), db, nullptr);

  ExpectValues ({{"one", "initial"}, {"four", "changed"}});
}

/* ************************************************************************** */

class SQLiteHasherTests : public SQLiteProcTests
{

protected:

  SQLiteHasher hasher;

  SQLiteHasherTests ()
  {
    hasher.SetupSchema (db);

    /* We run at every block for simplicity.  The interval filtering
       is tested already at the general level.  */
    hasher.SetInterval (1);
  }

  void
  ExpectHashes (const std::map<std::string, uint256>& expected)
  {
    ExpectRecords (db, "xayagame_statehashes", "hash", expected);
  }

};

TEST_F (SQLiteHasherTests, Works)
{
  SetValue ("foo");
  SHA256 hasher1;
  WriteAllTables (hasher1, db);
  const uint256 hash1 = hasher1.Finalise ();

  hasher.Process (BlockData (0, "zero"), db, nullptr);
  hasher.Process (BlockData (1, "one"), db, nullptr);

  SetValue ("bar");
  SHA256 hasher2;
  WriteAllTables (hasher2, db);
  const uint256 hash2 = hasher2.Finalise ();

  hasher.Process (BlockData (2, "two"), db, nullptr);

  ExpectHashes ({{"zero", hash1}, {"one", hash1}, {"two", hash2}});

  uint256 value;
  ASSERT_TRUE (hasher.GetHash (db, SHA256::Hash ("zero"), value));
  EXPECT_EQ (value, hash1);
  ASSERT_TRUE (hasher.GetHash (db, SHA256::Hash ("two"), value));
  EXPECT_EQ (value, hash2);
  ASSERT_FALSE (hasher.GetHash (db, SHA256::Hash ("foo"), value));
}

TEST_F (SQLiteHasherTests, SameBlock)
{
  SetValue ("foo");
  hasher.Process (BlockData (0, "zero"), db, nullptr);

  SetValue ("bar");
  EXPECT_DEATH (hasher.Process (BlockData (0, "zero"), db, nullptr),
                "differs from computed");
  hasher.Process (BlockData (0, "other zero"), db, nullptr);

  SetValue ("foo");
  hasher.Process (BlockData (0, "zero"), db, nullptr);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
