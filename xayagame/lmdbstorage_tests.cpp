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
 * Tests for things specific to LMDB.  The fixture manages a temporary directory
 * but otherwise leaves handling of the LMDBStorage to the test itself.
 */
class LMDBStorageTests : public testing::Test
{

private:

  TemporaryDirectory dir;

protected:

  const std::string
  GetDir () const
  {
    return dir.GetPath ();
  }

};

TEST_F (LMDBStorageTests, PersistsData)
{
  uint256 hash;
  CHECK (hash.FromHex ("99" + std::string (62, '0')));

  const GameStateData state = "some game state";
  const UndoData undo = "some undo data";

  {
    LMDBStorage storage(GetDir ());
    storage.Initialise ();

    storage.BeginTransaction ();
    storage.SetCurrentGameState (hash, state);
    storage.AddUndoData (hash, 42, undo);
    storage.CommitTransaction ();
  }

  {
    LMDBStorage storage(GetDir ());
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

TEST_F (LMDBStorageTests, ResizingMap)
{
  LMDBStorage storage(GetDir ());
  storage.Initialise ();

  /* The default map size is 1 MiB.  Each undo entry has at least a size of
     64 bytes, as that corresponds to the raw data of block hash and
     undo string.  So writing 2^20 / 2^6 = 2^14 undo entries to the
     map certainly exceeds the size and requires that the database handles
     resizing by itself.  */
  unsigned resized = 0;
  for (unsigned i = 0; i < (1 << 14); ++i)
    {
      std::string hex(64, '0');
      std::sprintf (&hex[0], "%08x", i);
      CHECK (hex[8] == 0);
      hex[8] = '0';

      uint256 hash;
      CHECK (hash.FromHex (hex));

      bool success = false;
      while (!success)
        try
          {
            storage.BeginTransaction ();
            storage.AddUndoData (hash, i, UndoData (32, 'x'));
            storage.CommitTransaction ();
            success = true;
          }
        catch (const StorageInterface::RetryWithNewTransaction& exc)
          {
            storage.RollbackTransaction ();
            ++resized;
          }
    }

  LOG (INFO) << "Resized the LMDB map " << resized << " times";
  CHECK_GT (resized, 0);
}

} // anonymous namespace
} // namespace xaya
