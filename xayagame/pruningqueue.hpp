// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_PRUNINGQUEUE_HPP
#define XAYAGAME_PRUNINGQUEUE_HPP

/* This file is an implementation detail of Game and should not be
   used directly by external code!  */

#include "storage.hpp"
#include "transactionmanager.hpp"

#include <xayautil/uint256.hpp>

#include <deque>

namespace xaya
{
namespace internal
{

/**
 * A queue of the last few block hashes in the blockchain, which helps us
 * implement pruning.
 */
class PruningQueue
{

private:

  /**
   * The storage used by the game, on which pruning methods will be called
   * as appropriate.
   */
  StorageInterface& storage;

  /**
   * The transaction manager that is used for starting/committing transactions
   * when changing the storage.
   */
  TransactionManager& transactionManager;

  /** The desired number of blocks to keep before pruning.  */
  unsigned nBlocks;

  /**
   * The queue of the last block hashes (front is oldest).  It is not actually
   * a queue, since we need to remove elements both from the front (old blocks
   * are pruned) and the back (reorgs).
   */
  std::deque<uint256> hashes;

  /**
   * Set to true if PruneUndoData has already been called on the storage
   * after the queue was filled up, i.e. if we are in "ongoing operation"
   * where we just prune individual blocks as they drop out of the queue.
   */
  bool initialPruningDone = false;

  /**
   * Performs the actual pruning if the queue is longer than necessary.
   */
  void PruneIfTooLong ();

public:

  /**
   * Creates a new pruning queue for the given storage reference and desired
   * number of blocks to keep.  The queue is empty at the beginning.
   */
  explicit PruningQueue (StorageInterface& s, TransactionManager& tm,
                         unsigned n);

  /**
   * Changes the number of desired blocks.  If the new value is smaller
   * than the current size of the queue, pruning is done to bring the size
   * down.  If the new value is larger, then nothing happens until more blocks
   * have been attached.
   */
  void SetDesiredSize (unsigned n);

  /**
   * Resets the queue to empty.  This can be used if the state got out of sync,
   * e.g. with missed ZMQ notifications.  In that case, we should rather start
   * filling the queue from scratch instead of risking a wrong prune.
   */
  void Reset ();

  /**
   * Adds a new block to the back of the queue (on top of the other
   * blocks).  If this enables pruning of older blocks, that is done through
   * the storage afterwards.
   */
  void AttachBlock (const uint256& hash, unsigned height);

  /**
   * Removes the "top" block (during a reorg).  If the queue is empty, this
   * is still fine to do and has no effect.
   */
  void DetachBlock ();

};

} // namespace internal
} // namespace xaya

#endif // XAYAGAME_PRUNINGQUEUE_HPP
