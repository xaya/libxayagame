// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqlitestorage.hpp"

#include "storage_tests.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <memory>

namespace xaya
{
namespace
{

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

} // anonymous namespace
} // namespace xaya
