// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pruningqueue.hpp"

#include "storage.hpp"
#include "uint256.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace internal
{
namespace
{

class PruningQueueTests : public testing::Test
{

protected:

  /** Memory storage used to test if it gets pruned.  */
  MemoryStorage storage;

  /** Next height of the simulated blockchain.  */
  unsigned nextHeight = 0;

  PruningQueueTests ()
  {
    /* Store some undo data into storage, just in case.  The existence
       of more data than we attach blocks in the test does not hurt, as we
       explicitly retrieve and verify only data in a specified range anyway.  */
    InitUnprunedState (200);
  }

  /**
   * Initialises the storage to an unpruned state with undo data from the
   * genesis block up to the given height.
   */
  void
  InitUnprunedState (const unsigned height)
  {
    storage.Clear ();
    for (unsigned i = 0; i <= height; ++i)
      {
        const uint256 hash = BlockHash (i);
        storage.AddUndoData (hash, i, hash.ToHex ());
      }
  }

  /**
   * Asserts in the test that all blocks up to (including) the given height
   * have been pruned.
   */
  void
  AssertPrunedUpTo (const unsigned height) const
  {
    for (unsigned i = 0; i <= height; ++i)
      {
        UndoData dummyUndo;
        ASSERT_FALSE (storage.GetUndoData (BlockHash (i), dummyUndo))
            << "Undo data for height " << i
            << " is available but should have been pruned";
      }
  }

  /**
   * Asserts that undo data for heights in the given interval (both inclusive)
   * is available and not pruned.
   */
  void
  AssertNotPrunedBetween (const unsigned from, const unsigned to) const
  {
    for (unsigned i = from; i <= to; ++i)
      {
        UndoData dummyUndo;
        ASSERT_TRUE (storage.GetUndoData (BlockHash (i), dummyUndo))
            << "Undo data for height " << i
            << " is not available but should be";
      }
  }

  /**
   * Asserts that the first non-pruned block is the given height.  In other
   * words, that blocks [0, k) are pruned and [k, nextHeight) are not.
   */
  void
  AssertFirstNonPruned (const unsigned k) const
  {
    if (k > 0)
      AssertPrunedUpTo (k - 1);
    AssertNotPrunedBetween (k, nextHeight - 1);
  }

  /**
   * Attaches the next k blocks (from nextHeight) to the queue.
   */
  void
  AttachBlocks (PruningQueue& q, const unsigned k)
  {
    for (unsigned i = 0; i < k; ++i)
      {
        q.AttachBlock (BlockHash (nextHeight), nextHeight);
        ++nextHeight;
      }
  }

  /**
   * Detaches k blocks from the queue and nextHeight.
   */
  void
  DetachBlocks (PruningQueue& q, const unsigned k)
  {
    CHECK_LE (k, nextHeight);
    for (unsigned i = 0; i < k; ++i)
      {
        q.DetachBlock ();
        --nextHeight;
      }
  }

};

TEST_F (PruningQueueTests, AttachingFromGenesis)
{
  PruningQueue queue(storage, 10);

  AttachBlocks (queue, 100);
  AssertFirstNonPruned (90);
}

TEST_F (PruningQueueTests, InitialPruning)
{
  PruningQueue queue(storage, 10);

  nextHeight = 10;
  AttachBlocks (queue, 9);
  AssertFirstNonPruned (0);

  AttachBlocks (queue, 1);
  AssertFirstNonPruned (10);
}

TEST_F (PruningQueueTests, DetachingBeforeStart)
{
  PruningQueue queue(storage, 10);

  nextHeight = 10;
  AttachBlocks (queue, 1);
  DetachBlocks (queue, 2);

  AttachBlocks (queue, 9);
  AssertFirstNonPruned (0);

  AttachBlocks (queue, 1);
  AssertFirstNonPruned (9);
}

TEST_F (PruningQueueTests, DetachingDuringOperation)
{
  PruningQueue queue(storage, 10);

  AttachBlocks (queue, 50);
  AssertFirstNonPruned (40);

  DetachBlocks (queue, 5);
  AttachBlocks (queue, 5);
  AssertFirstNonPruned (40);

  AttachBlocks (queue, 1);
  AssertFirstNonPruned (41);
}

TEST_F (PruningQueueTests, Reset)
{
  PruningQueue queue(storage, 10);

  AttachBlocks (queue, 50);
  AssertFirstNonPruned (40);

  InitUnprunedState (200);
  queue.Reset ();

  AttachBlocks (queue, 9);
  AssertFirstNonPruned (0);
  AttachBlocks (queue, 1);
  AssertFirstNonPruned (50);
}

using SetDesiredSizeTests = PruningQueueTests;

TEST_F (SetDesiredSizeTests, MakeLarger)
{
  PruningQueue queue(storage, 10);

  AttachBlocks (queue, 50);
  AssertFirstNonPruned (40);

  queue.SetDesiredSize (15);
  AttachBlocks (queue, 5);
  AssertFirstNonPruned (40);

  AttachBlocks (queue, 1);
  AssertFirstNonPruned (41);
}

TEST_F (SetDesiredSizeTests, TriggersInitialPruning)
{
  PruningQueue queue(storage, 10);

  nextHeight = 10;
  AttachBlocks (queue, 9);
  AssertFirstNonPruned (0);

  /* Just setting the desired size by itself cannot trigger initial pruning,
     since that needs information about the current height which is only passed
     to AttachBlocks.  So the initial pruning is triggered together with the
     next attach operation.  */
  queue.SetDesiredSize (5);
  DetachBlocks (queue, 1);
  AttachBlocks (queue, 1);
  AssertFirstNonPruned (14);
}

TEST_F (SetDesiredSizeTests, TriggersRegularPruning)
{
  PruningQueue queue(storage, 10);

  AttachBlocks (queue, 50);
  AssertFirstNonPruned (40);

  queue.SetDesiredSize (5);
  AssertFirstNonPruned (45);
}

class ZeroNBlocksTests : public PruningQueueTests
{

protected:

  /**
   * Asserts that all blocks have been pruned, as is always expected
   * for nBlocks = 0.
   */
  void
  AssertAllPruned () const
  {
    AssertPrunedUpTo (nextHeight - 1);
  }

};

TEST_F (ZeroNBlocksTests, FromGenesis)
{
  PruningQueue queue(storage, 0);

  AttachBlocks (queue, 10);
  AssertAllPruned ();
}

TEST_F (ZeroNBlocksTests, LateStart)
{
  PruningQueue queue(storage, 0);

  nextHeight = 10;
  AttachBlocks (queue, 1);
  AssertAllPruned ();
}

TEST_F (ZeroNBlocksTests, Detaches)
{
  PruningQueue queue(storage, 0);

  AttachBlocks (queue, 10);
  AssertAllPruned ();

  DetachBlocks (queue, 5);
  AssertAllPruned ();

  AttachBlocks (queue, 10);
  AssertAllPruned ();
}

} // anonymous namespace
} // namespace internal
} // namespace xaya
