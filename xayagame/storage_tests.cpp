// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "storage.hpp"

#include "uint256.hpp"

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

/* TODO:  Turn this into a typed or type-parametrised test once we have
   more storage implementations.  It just verifies the StorageInterface
   and nothing specific to the in-memory storage.  */
class MemoryStorageTests : public testing::Test
{

protected:

  uint256 hash1, hash2;

  const GameStateData state1 = "state 1";
  const GameStateData state2 = "state 2";

  const UndoData undo1 = "undo 1";
  const UndoData undo2 = "undo 2";

  MemoryStorage storage;

  MemoryStorageTests ()
  {
    CHECK (hash1.FromHex ("01" + std::string (62, '0')));
    CHECK (hash2.FromHex ("02" + std::string (62, '0')));
  }

};

TEST_F (MemoryStorageTests, Empty)
{
  uint256 hash;
  EXPECT_FALSE (storage.GetCurrentBlockHash (hash));
  UndoData undo;
  EXPECT_FALSE (storage.GetUndoData (hash1, undo));
}

TEST_F (MemoryStorageTests, CurrentState)
{
  uint256 hash;

  storage.SetCurrentGameState (hash1, state1);
  ASSERT_TRUE (storage.GetCurrentBlockHash (hash));
  EXPECT_EQ (hash, hash1);
  EXPECT_EQ (storage.GetCurrentGameState (), state1);

  storage.SetCurrentGameState (hash2, state2);
  ASSERT_TRUE (storage.GetCurrentBlockHash (hash));
  EXPECT_EQ (hash, hash2);
  EXPECT_EQ (storage.GetCurrentGameState (), state2);
}

TEST_F (MemoryStorageTests, UndoData)
{
  UndoData undo;
  EXPECT_FALSE (storage.GetUndoData (hash1, undo));

  storage.AddUndoData (hash1, undo1);
  ASSERT_TRUE (storage.GetUndoData (hash1, undo));
  EXPECT_EQ (undo, undo1);
  EXPECT_FALSE (storage.GetUndoData (hash2, undo));

  /* Adding twice should be fine (just have no effect but also not crash).  */
  storage.AddUndoData (hash1, undo1);

  storage.AddUndoData (hash2, undo2);
  ASSERT_TRUE (storage.GetUndoData (hash1, undo));
  EXPECT_EQ (undo, undo1);
  ASSERT_TRUE (storage.GetUndoData (hash2, undo));
  EXPECT_EQ (undo, undo2);

  /* Removing should be ok (not crash), but otherwise no effect is guaranteed
     (in particular, not that it will actually be removed).  */
  storage.RemoveUndoData (hash1);
  ASSERT_TRUE (storage.GetUndoData (hash2, undo));
  EXPECT_EQ (undo, undo2);
  storage.RemoveUndoData (hash2);
}

TEST_F (MemoryStorageTests, Clear)
{
  storage.SetCurrentGameState (hash1, state1);
  storage.AddUndoData (hash1, undo1);

  uint256 hash;
  EXPECT_TRUE (storage.GetCurrentBlockHash (hash));
  UndoData undo;
  EXPECT_TRUE (storage.GetUndoData (hash1, undo));

  storage.Clear ();
  EXPECT_FALSE (storage.GetCurrentBlockHash (hash));
  EXPECT_FALSE (storage.GetUndoData (hash1, undo));
}

} // anonymous namespace
} // namespace xaya
