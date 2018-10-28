// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "lmdbstorage.hpp"

#include "storage_tests.hpp"

#include "storage.hpp"
#include "uint256.hpp"

#include <gtest/gtest.h>

#include <experimental/filesystem>

#include <cstdio>

namespace xaya
{
namespace
{

namespace fs = std::experimental::filesystem;

/**
 * Creates a temporary directory and removes it again in the destructor.
 */
class TemporaryDirectory
{

private:

  /** Path of the created directory.  */
  fs::path dir;

public:

  TemporaryDirectory ()
  {
    dir = std::tmpnam (nullptr);
    LOG (INFO) << "Temporary directory for LMDB: " << dir;
    CHECK (fs::create_directories (dir));
  }

  ~TemporaryDirectory ()
  {
    LOG (INFO) << "Cleaning up temporary directory: " << dir;
    fs::remove_all (dir);
  }

  const std::string
  GetPath () const
  {
    return dir.string ();
  }

};

/**
 * Helper class that wraps LMDBStorage but also manages a temporary data
 * directory for the database.  We cannot simply extend LMDBStorage, as that
 * would give us the wrong relative order of construction/destruction for the
 * storage and directory.
 */
class TempLMDBStorage : public StorageInterface
{

private:

  /** The temporary directory that is used.  */
  TemporaryDirectory tempDir;

  /** The LMDBStorage object itself.  */
  LMDBStorage storage;

public:

  TempLMDBStorage ()
    : tempDir(), storage(tempDir.GetPath ())
  {}

  void
  Initialise () override
  {
    storage.Initialise ();
  }

  void
  Clear () override
  {
    storage.Clear ();
  }

  bool
  GetCurrentBlockHash (uint256& hash) const override
  {
    return storage.GetCurrentBlockHash (hash);
  }

  GameStateData
  GetCurrentGameState () const override
  {
    return storage.GetCurrentGameState ();
  }

  void
  SetCurrentGameState (const uint256& hash, const GameStateData& data) override
  {
    storage.SetCurrentGameState (hash, data);
  }

  bool
  GetUndoData (const uint256& hash, UndoData& data) const override
  {
    return storage.GetUndoData (hash, data);
  }

  void
  AddUndoData (const uint256& hash,
               const unsigned height, const UndoData& data) override
  {
    storage.AddUndoData (hash, height, data);
  }

  void
  ReleaseUndoData (const uint256& hash) override
  {
    storage.ReleaseUndoData (hash);
  }

  void
  PruneUndoData (const unsigned height) override
  {
    storage.PruneUndoData (height);
  }

  void
  BeginTransaction () override
  {
    storage.BeginTransaction ();
  }

  void
  CommitTransaction () override
  {
    storage.CommitTransaction ();
  }

  void
  RollbackTransaction () override
  {
    storage.RollbackTransaction ();
  }

};

INSTANTIATE_TYPED_TEST_CASE_P (LMDB, BasicStorageTests,
                               TempLMDBStorage);
INSTANTIATE_TYPED_TEST_CASE_P (LMDB, PruningStorageTests,
                               TempLMDBStorage);
INSTANTIATE_TYPED_TEST_CASE_P (LMDB, TransactingStorageTests,
                               TempLMDBStorage);

/**
 * Tests the LMDBStorage's persistence between closing and reopening the
 * database in a given directory.
 */
TEST (PersistentLMDBStorageTests, PersistsData)
{
  TemporaryDirectory dir;

  uint256 hash;
  CHECK (hash.FromHex ("99" + std::string (62, '0')));

  const GameStateData state = "some game state";
  const UndoData undo = "some undo data";

  {
    LMDBStorage storage(dir.GetPath ());
    storage.Initialise ();

    storage.BeginTransaction ();
    storage.SetCurrentGameState (hash, state);
    storage.AddUndoData (hash, 42, undo);
    storage.CommitTransaction ();
  }

  {
    LMDBStorage storage(dir.GetPath ());
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

} // anonymous namespace
} // namespace xaya
