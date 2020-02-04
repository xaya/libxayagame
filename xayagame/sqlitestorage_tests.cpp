// Copyright (C) 2018-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqlitestorage.hpp"

#include "storage_tests.hpp"
#include "testutils.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
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
    auto* stmt = db.PrepareRo (R"(
      SELECT `value`
        FROM `xayagame_current`
        WHERE `key` = 'gamestate'
    )");

    const int rc = sqlite3_step (stmt);
    if (rc == SQLITE_DONE)
      {
        EXPECT_EQ (value, "") << "No state in the database, expected " << value;
        return;
      }
    CHECK_EQ (rc, SQLITE_ROW);

    const unsigned char* data = sqlite3_column_text (stmt, 0);
    EXPECT_EQ (std::string (reinterpret_cast<const char*> (data)), value);

    CHECK_EQ (sqlite3_step (stmt), SQLITE_DONE);
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
  auto* stmt = snapshot->Prepare (R"(
    INSERT INTO `xayagame_current`
      (`key`, `value`) VALUES ('foo', 'bar')
  )");
  EXPECT_EQ (sqlite3_step (stmt), SQLITE_READONLY);
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

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
