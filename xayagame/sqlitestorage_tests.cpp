// Copyright (C) 2018-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqlitestorage.hpp"

#include "storage_tests.hpp"
#include "testutils.hpp"

#include "xayautil/hash.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <limits>
#include <memory>
#include <thread>

namespace xaya
{
namespace
{

/* ************************************************************************** */

/**
 * Simple utility subclass of SQLiteStorage, which makes it default
 * constructible with an in-memory database.
 */
class InMemorySQLiteStorage : public SQLiteStorage
{

public:

  InMemorySQLiteStorage ()
    : SQLiteStorage (":memory:")
  {}

};

INSTANTIATE_TYPED_TEST_CASE_P (SQLite, BasicStorageTests,
                               InMemorySQLiteStorage);
INSTANTIATE_TYPED_TEST_CASE_P (SQLite, PruningStorageTests,
                               InMemorySQLiteStorage);
INSTANTIATE_TYPED_TEST_CASE_P (SQLite, TransactingStorageTests,
                               InMemorySQLiteStorage);

/* ************************************************************************** */

class SQLiteStatementTests : public testing::Test
{

protected:

  SQLiteDatabase db;

  SQLiteStatementTests ()
    : db("foo", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY)
  {
    db.Execute (R"(
      CREATE TABLE `test`
        (`int` INTEGER NULL,
         `text` TEXT NULL,
         `blob` BLOB NULL);
    )");
  }

  /**
   * Tests a "roundtrip" of storing a value into our test database
   * with a prepared statement parameter bind, and then extracting
   * it again to test that the binding and extraction works.
   */
  template <typename T>
    void
    Roundtrip (const std::string& column, const T& val)
  {
    auto stmt = db.Prepare (R"(
      INSERT INTO `test`
        (`)" + column + R"(`) VALUES (?1)
    )");
    stmt.Bind (1, val);
    stmt.Execute ();

    stmt = db.PrepareRo (R"(
      SELECT `)" + column + R"(` FROM `test`
    )");

    ASSERT_TRUE (stmt.Step ());
    EXPECT_EQ (stmt.Get<T> (0), val);
    ASSERT_FALSE (stmt.Step ());

    stmt = db.Prepare (R"(
      DELETE FROM `test`
    )");
    stmt.Execute ();
  }

};

TEST_F (SQLiteStatementTests, BindExtract)
{
  Roundtrip<int> ("int", 0);
  Roundtrip<int> ("int", 123);
  Roundtrip<int> ("int", -5);

  Roundtrip<unsigned> ("int", 0);
  Roundtrip<unsigned> ("int", 1 << 20);

  Roundtrip<int64_t> ("int", 0);
  Roundtrip<int64_t> ("int", std::numeric_limits<int64_t>::min ());
  Roundtrip<int64_t> ("int", std::numeric_limits<int64_t>::max ());

  Roundtrip<uint64_t> ("int", 0);
  Roundtrip<uint64_t> ("int", std::numeric_limits<int64_t>::max ());

  Roundtrip<bool> ("int", false);
  Roundtrip<bool> ("int", true);

  Roundtrip<std::string> ("text", "");
  Roundtrip<std::string> ("text", "foobar");
  Roundtrip<std::string> ("text", u8"äöü");
  Roundtrip<std::string> ("text", std::string ("abc\0def", 7));

  uint256 zero;
  zero.SetNull ();
  Roundtrip<uint256> ("blob", zero);
  Roundtrip<uint256> ("blob", SHA256::Hash ("foobar"));
}

TEST_F (SQLiteStatementTests, BindCheckNull)
{
  auto stmt = db.Prepare (R"(
    INSERT INTO `test`
      (`int`, `text`) VALUES (1, ?1), (2, ?2)
  )");
  stmt.Bind<std::string> (1, "foo");
  stmt.Bind<std::string> (2, "wrong");
  stmt.BindNull (2);
  stmt.Execute ();

  stmt = db.PrepareRo (R"(
    SELECT `text`
      FROM `test`
      ORDER BY `int`
  )");
  ASSERT_TRUE (stmt.Step ());
  ASSERT_FALSE (stmt.IsNull (0));
  EXPECT_EQ (stmt.Get<std::string> (0), "foo");
  ASSERT_TRUE (stmt.Step ());
  EXPECT_TRUE (stmt.IsNull (0));
  ASSERT_FALSE (stmt.Step ());
}

TEST_F (SQLiteStatementTests, BindExtractBlob)
{
  std::string binary;
  for (int i = 0; i <= 0xFF; ++i)
    binary.push_back (static_cast<char> (i));

  auto stmt = db.Prepare (R"(
    INSERT INTO `test`
      (`int`, `blob`) VALUES (1, ?1), (2, ?2)
  )");
  stmt.BindBlob (1, "");
  stmt.BindBlob (2, binary);
  stmt.Execute ();

  stmt = db.PrepareRo (R"(
    SELECT `blob`
      FROM `test`
      ORDER BY `int`
  )");
  ASSERT_TRUE (stmt.Step ());
  EXPECT_EQ (stmt.GetBlob (0), "");
  ASSERT_TRUE (stmt.Step ());
  EXPECT_EQ (stmt.GetBlob (0), binary);
  ASSERT_FALSE (stmt.Step ());
}

TEST_F (SQLiteStatementTests, Reset)
{
  auto stmt = db.Prepare (R"(
    INSERT INTO `test`
      (`int`, `text`)
      VALUES
        (1, 'foo'),
        (1, 'bar'),
        (2, 'baz')
  )");
  stmt.Execute ();

  stmt = db.PrepareRo (R"(
    SELECT `text`
      FROM `test`
      WHERE `int` = ?1
      ORDER BY `text`
  )");
  stmt.Bind (1, 1);

  for (int i = 0; i < 10; ++i)
    {
      stmt.Reset ();
      ASSERT_TRUE (stmt.Step ());
      EXPECT_EQ (stmt.Get<std::string> (0), "bar");
      ASSERT_TRUE (stmt.Step ());
      EXPECT_EQ (stmt.Get<std::string> (0), "foo");
      ASSERT_FALSE (stmt.Step ());
    }

  stmt.Reset ();
  stmt.Bind (1, 2);
  ASSERT_TRUE (stmt.Step ());
  EXPECT_EQ (stmt.Get<std::string> (0), "baz");
  ASSERT_FALSE (stmt.Step ());
}

TEST_F (SQLiteStatementTests, ConcurrentStatementUse)
{
  /* In this test, we ensure that the statement cache is able to handle
     both concurrent threads accessing the database and also a single
     thread using a single statement twice at the same time (which should
     yield two independent sqlite3_stmt's).  */

  constexpr unsigned numThreads = 10;

  auto stmt = db.Prepare (R"(
    INSERT INTO `test`
      (`int`)
      VALUES (1), (2), (3)
  )");
  stmt.Execute ();

  const std::string sql = R"(
    SELECT `int`
      FROM `test`
      ORDER BY `int`
  )";

  constexpr auto waitTime = std::chrono::milliseconds (100);
  using Clock = std::chrono::steady_clock;
  const auto before = Clock::now ();

  std::vector<std::thread> threads;
  for (unsigned i = 0; i < numThreads; ++i)
    threads.emplace_back ([this, waitTime, &sql] ()
      {
        auto stmt1 = db.PrepareRo (sql);
        ASSERT_TRUE (stmt1.Step ());
        ASSERT_EQ (stmt1.Get<int64_t> (0), 1);

        auto stmt2 = db.PrepareRo (sql);
        for (int j = 1; j <= 3; ++j)
          {
            ASSERT_TRUE (stmt2.Step ());
            ASSERT_EQ (stmt2.Get<int64_t> (0), j);
          }

        std::this_thread::sleep_for (waitTime);

        ASSERT_TRUE (stmt1.Step ());
        ASSERT_EQ (stmt1.Get<int64_t> (0), 2);
        ASSERT_TRUE (stmt1.Step ());
        ASSERT_EQ (stmt1.Get<int64_t> (0), 3);

        ASSERT_FALSE (stmt1.Step ());
        ASSERT_FALSE (stmt2.Step ());
      });
  for (auto& t : threads)
    t.join ();

  const auto after = Clock::now ();
  CHECK (after - before < 2 * waitTime);
}

/* ************************************************************************** */

/**
 * Tests for SQLiteStorage with a temporary on-disk database file (instead of
 * just an in-memory database).  They verify explicitly that data is persisted
 * for a second instantiation of SQLiteStorage that uses the same file.
 */
class PersistentSQLiteStorageTests : public testing::Test
{

protected:

  /** Example uint256 value for use as block hash in the test.  */
  uint256 hash;

  /** Example game state value.  */
  const GameStateData state = "some game state";

  /** Example undo data value.  */
  const UndoData undo = "some undo data";

  /** Name of the temporary file used for the database.  */
  std::string filename;

  PersistentSQLiteStorageTests ()
  {
    CHECK (hash.FromHex ("99" + std::string (62, '0')));

    filename = std::tmpnam (nullptr);
    LOG (INFO) << "Using temporary database file: " << filename;
  }

  ~PersistentSQLiteStorageTests ()
  {
    LOG (INFO) << "Cleaning up temporary file: " << filename;
    std::remove (filename.c_str ());
  }

};

TEST_F (PersistentSQLiteStorageTests, PersistsData)
{
  {
    SQLiteStorage storage(filename);
    storage.Initialise ();

    storage.BeginTransaction ();
    storage.SetCurrentGameState (hash, state);
    storage.AddUndoData (hash, 42, undo);
    storage.CommitTransaction ();
  }

  {
    SQLiteStorage storage(filename);
    storage.Initialise ();

    uint256 h;
    ASSERT_TRUE (storage.GetCurrentBlockHash (h));
    EXPECT_TRUE (h == hash);
    EXPECT_EQ (storage.GetCurrentGameState (), state);

    UndoData val;
    ASSERT_TRUE (storage.GetUndoData (hash, val));
    EXPECT_EQ (val, undo);
  }
}

TEST_F (PersistentSQLiteStorageTests, ClearWithOnDiskFile)
{
  SQLiteStorage storage(filename);
  storage.Initialise ();

  storage.BeginTransaction ();
  storage.SetCurrentGameState (hash, state);
  storage.AddUndoData (hash, 42, undo);
  storage.CommitTransaction ();

  uint256 h;
  UndoData val;
  EXPECT_TRUE (storage.GetCurrentBlockHash (h));
  EXPECT_TRUE (storage.GetUndoData (hash, val));

  storage.Clear ();
  EXPECT_FALSE (storage.GetCurrentBlockHash (h));
  EXPECT_FALSE (storage.GetUndoData (hash, val));
}

/* ************************************************************************** */

class SQLiteStorageSnapshotTests : public PersistentSQLiteStorageTests
{

protected:

  class Storage : public SQLiteStorage
  {

  public:

    using SQLiteStorage::SQLiteStorage;
    using SQLiteStorage::GetDatabase;
    using SQLiteStorage::GetSnapshot;

  };

  /**
   * Expects that the current game state as seen by the given database
   * matches the given value.  The empty value matches "no state".
   */
  void
  ExpectDatabaseState (const SQLiteDatabase& db, const std::string& value)
  {
    auto stmt = db.PrepareRo (R"(
      SELECT `value`
        FROM `xayagame_current`
        WHERE `key` = 'gamestate'
    )");

    if (!stmt.Step ())
      {
        EXPECT_EQ (value, "") << "No state in the database, expected " << value;
        return;
      }

    EXPECT_EQ (stmt.Get<std::string> (0), value);
    CHECK (!stmt.Step ());
  }

};

TEST_F (SQLiteStorageSnapshotTests, SnapshotNotSupported)
{
  Storage storage(":memory:");
  storage.Initialise ();

  EXPECT_EQ (storage.GetSnapshot (), nullptr);
}

TEST_F (SQLiteStorageSnapshotTests, SnapshotsAreReadonly)
{
  Storage storage(filename);
  storage.Initialise ();

  auto snapshot = storage.GetSnapshot ();
  ASSERT_NE (snapshot, nullptr);
  auto stmt = snapshot->Prepare (R"(
    INSERT INTO `xayagame_current`
      (`key`, `value`) VALUES ('foo', 'bar')
  )");
  EXPECT_EQ (sqlite3_step (*stmt), SQLITE_READONLY);
}

TEST_F (SQLiteStorageSnapshotTests, MultipleSnapshots)
{
  Storage storage(filename);
  storage.Initialise ();

  storage.BeginTransaction ();
  auto s1 = storage.GetSnapshot ();
  storage.SetCurrentGameState (hash, "first");
  storage.CommitTransaction ();

  auto s2 = storage.GetSnapshot ();
  storage.BeginTransaction ();
  storage.SetCurrentGameState (hash, "second");
  auto s3 = storage.GetSnapshot ();
  storage.CommitTransaction ();
  auto s4 = storage.GetSnapshot ();

  ExpectDatabaseState (*s1, "");
  ExpectDatabaseState (*s2, "first");
  ExpectDatabaseState (*s3, "first");
  ExpectDatabaseState (*s4, "second");
  ExpectDatabaseState (storage.GetDatabase (), "second");
}

TEST_F (SQLiteStorageSnapshotTests, CloseWaitsForOutstandingSnapshots)
{
  Storage storage(filename);
  storage.Initialise ();

  storage.BeginTransaction ();
  storage.SetCurrentGameState (hash, "state");
  storage.CommitTransaction ();

  auto s1 = storage.GetSnapshot ();
  auto s2 = storage.GetSnapshot ();

  std::atomic<bool> done(false);
  auto clearJob = std::make_unique<std::thread> ([&] ()
    {
      storage.Clear ();
      done = true;
    });

  SleepSome ();
  ExpectDatabaseState (*s1, "state");
  ExpectDatabaseState (*s2, "state");
  EXPECT_FALSE (done);

  s1.reset ();
  s2.reset ();
  clearJob->join ();

  ExpectDatabaseState (storage.GetDatabase (), "");
}

TEST_F (SQLiteStorageSnapshotTests, StatementsMustAllBeDestructed)
{
  Storage storage(filename);
  storage.Initialise ();

  storage.BeginTransaction ();
  storage.SetCurrentGameState (hash, "state");
  storage.CommitTransaction ();

  auto s = storage.GetSnapshot ();
  auto stmt = s->PrepareRo (R"(
    SELECT *
      FROM `xayagame_current`
  )");

  /* Destructing the database while the statement is still around
     should CHECK fail.  At the end of the test scope, the statement
     will be destructed before the snapshot, which is fine.  */
  EXPECT_DEATH (s.reset (), "statement is still in use");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
