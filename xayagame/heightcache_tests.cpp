// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "heightcache.hpp"

#include "storage.hpp"
#include "uint256.hpp"

#include "storage_tests.hpp"
#include "testutils.hpp"

#include <gtest/gtest.h>

#include <glog/logging.h>

#include <memory>

namespace xaya
{
namespace internal
{

/**
 * Modified instance of StorageWithCachedHeight that uses a dummy height,
 * so that it can be tested against the standard storage tests.
 */
class StorageWithDummyHeight : public StorageWithCachedHeight
{

private:

  /**
   * The memory storage that is wrapped by the cached-height storage.
   * We have to use a pointer here, since we need to construct that object
   * already before the super-class constructor is called.  The pointer is
   * then initialised from the reference in the super-class afterwards.
   * This also means that the storage will be destructed before the super-class
   * destructor is called, but that is fine since it is not used from there
   * (and this is only test code).
   */
  std::unique_ptr<StorageInterface> memoryStorage;

  /**
   * Dummy function for the hash-to-height translation.  This should not be
   * called at all for the standard storage tests.
   */
  static unsigned
  HashToHeight (const uint256& hash)
  {
    LOG (FATAL) << "HashToHeight should not be called for the storage tests";
  }

public:

  StorageWithDummyHeight ()
    : StorageWithCachedHeight (*new MemoryStorage (), &HashToHeight)
  {
    memoryStorage.reset (StorageWithCachedHeight::storage);
  }

  void
  SetCurrentGameState (const uint256& hash, const GameStateData& data) override
  {
    SetCurrentGameStateWithHeight (hash, 0, data);
  }

};

namespace
{

/* Verify that the wrapped storage works as a basic storage, if we just cache
   a dummy height (and never request the height).  */
INSTANTIATE_TYPED_TEST_CASE_P (HeightCache, BasicStorageTests,
                               StorageWithDummyHeight);
INSTANTIATE_TYPED_TEST_CASE_P (HeightCache, PruningStorageTests,
                               StorageWithDummyHeight);

/**
 * Test fixture that sets up a memory storage and a real storage with cached
 * height (not the dummy one).  This is used for tests of the height cache
 * itself.
 */
class HeightCacheTests : public testing::Test
{

private:

  /** The underlying memory storage.  */
  MemoryStorage memoryStorage;

  /**
   * Hash-to-height function that checks the hash against the first ten
   * test block hashes (from testutils' BlockHash).  This also increments
   * the hashToHeightCount value.
   */
  unsigned
  HashToHeight (const uint256& hash)
  {
    ++hashToHeightCount;
    for (unsigned i = 0; i < 10; ++i)
      if (hash == BlockHash (i))
        return i;
    LOG (FATAL) << "Unexpected test block hash: " << hash.ToHex ();
  }

protected:

  /** The height-caching storage that can be used for tests.  */
  StorageWithCachedHeight storage;

  /** Counter for how often the hash-to-height function has been called.  */
  int hashToHeightCount = 0;

  HeightCacheTests ()
    : memoryStorage(),
      storage(memoryStorage,
              [this] (const uint256& hash) {
                return HashToHeight (hash);
              })
  {}

  /**
   * Utility function to store the given hash and height.
   */
  void
  StoreHashAndHeight (const uint256& hash, const unsigned height)
  {
    storage.BeginTransaction ();
    storage.SetCurrentGameStateWithHeight (hash, height, GameStateData ());
    storage.CommitTransaction ();
  }

  /**
   * Stores the given hash as current game state in the underlying storage,
   * bypassing the cache.  This can be used to simulate a situation where
   * the storage has a persisted value but the cache has just been started.
   */
  void
  StoreOnlyHash (const uint256& hash)
  {
    memoryStorage.BeginTransaction ();
    memoryStorage.SetCurrentGameState (hash, GameStateData ());
    memoryStorage.CommitTransaction ();
  }

  /**
   * Expect the given hash and height as current state.
   */
  void
  ExpectHashAndHeight (const uint256& expectedHash,
                       const unsigned expectedHeight) const
  {
    uint256 hash;
    unsigned height;
    ASSERT_TRUE (storage.GetCurrentBlockHashWithHeight (hash, height));
    EXPECT_EQ (hash, expectedHash);
    EXPECT_EQ (height, expectedHeight);
  }

};

TEST_F (HeightCacheTests, NoCurrentState)
{
  uint256 hash;
  unsigned height;
  EXPECT_FALSE (storage.GetCurrentBlockHashWithHeight (hash, height));
  EXPECT_EQ (hashToHeightCount, 0);
}

TEST_F (HeightCacheTests, BasicCaching)
{
  StoreHashAndHeight (BlockHash (2), 10);
  ExpectHashAndHeight (BlockHash (2), 10);
  EXPECT_EQ (hashToHeightCount, 0);
}

TEST_F (HeightCacheTests, TranslationFunction)
{
  StoreOnlyHash (BlockHash (2));
  ExpectHashAndHeight (BlockHash (2), 2);
  EXPECT_EQ (hashToHeightCount, 1);
}

TEST_F (HeightCacheTests, CrossChecks)
{
  storage.EnableCrossChecks ();
  StoreHashAndHeight (BlockHash (2), 10);

  uint256 hash;
  unsigned height;
  EXPECT_DEATH (storage.GetCurrentBlockHashWithHeight (hash, height),
                "Cached height is wrong");
}

TEST_F (HeightCacheTests, Clear)
{
  StoreHashAndHeight (BlockHash (2), 10);
  storage.Clear ();
  StoreOnlyHash (BlockHash (2));

  ExpectHashAndHeight (BlockHash (2), 2);
  EXPECT_EQ (hashToHeightCount, 1);
}

TEST_F (HeightCacheTests, RollbackTransaction)
{
  storage.BeginTransaction ();
  storage.SetCurrentGameStateWithHeight (BlockHash (2), 10, GameStateData ());
  storage.RollbackTransaction ();

  StoreOnlyHash (BlockHash (2));

  ExpectHashAndHeight (BlockHash (2), 2);
  EXPECT_EQ (hashToHeightCount, 1);
}

TEST_F (HeightCacheTests, NoSettingWithoutHeight)
{
  storage.BeginTransaction ();
  EXPECT_DEATH (storage.SetCurrentGameState (BlockHash (2), GameStateData ()),
                "SetCurrentGameStateWithHeight");
  storage.CommitTransaction ();
}

} // anonymous namespace
} // namespace internal
} // namespace xaya
