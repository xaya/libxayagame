// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pruningqueue.hpp"

#include <glog/logging.h>

namespace xaya
{
namespace internal
{

PruningQueue::PruningQueue (StorageInterface& s, TransactionManager& tm,
                            const unsigned n)
  : storage(s), transactionManager(tm), nBlocks(n)
{
  LOG (INFO) << "Created empty pruning queue with desired size " << nBlocks;
}

void
PruningQueue::PruneIfTooLong ()
{
  VLOG (1)
      << "Pruning queue has " << hashes.size ()
      << " entries, the desired size is " << nBlocks;

  if (hashes.size () <= nBlocks)
    return;

  VLOG (1) << "Pruning " << (hashes.size () - nBlocks) << " old blocks";

  /* We use just one transaction for pruning all blocks to avoid excessive
     transaction creation.  This means that if a failure occurs, it may be
     that the deletion of data from the storage is rolled back while the
     in-memory queue thinks it is deleted.  But that is no big deal, as
     we will re-prune anyway on the next startup at the latest.  */
  ActiveTransaction tx(transactionManager);
  while (hashes.size () > nBlocks)
    {
      storage.ReleaseUndoData (hashes.front ());
      hashes.pop_front ();
    }
  tx.SetSuccess ();
}

void
PruningQueue::SetDesiredSize (const unsigned n)
{
  LOG (INFO)
      << "Changing desired size of pruning queue from " << nBlocks
      << " to " << n;
  nBlocks = n;
  PruneIfTooLong ();
}

void
PruningQueue::Reset ()
{
  LOG (INFO) << "Resetting pruning queue";
  hashes.clear ();
  initialPruningDone = false;
}

void
PruningQueue::AttachBlock (const uint256& hash, const unsigned height)
{
  VLOG (1) << "Attaching block to pruning queue: " << hash.ToHex ();
  hashes.push_back (hash);

  if (!initialPruningDone && hashes.size () >= nBlocks)
    {
      /* Start by computing the height of the *front* (oldest) block in the
         queue.  This is guaranteed to be non-negative, as we had at least
         as many attaches in a row as the size of the queue.  */
      CHECK_GE (height + 1, hashes.size ());
      const unsigned frontHeight = height + 1 - hashes.size ();

      LOG (INFO)
          << "Pruning queue has filled up, removing all old blocks before"
             " the front height " << frontHeight;

      if (frontHeight > 0)
        {
          ActiveTransaction tx(transactionManager);
          storage.PruneUndoData (frontHeight - 1);
          tx.SetSuccess ();
        }
      initialPruningDone = true;
    }

  PruneIfTooLong ();
}

void
PruningQueue::DetachBlock ()
{
  if (hashes.empty ())
    {
      /* There are two situations in which this may happen:  First, if the
         queue is empty because the node was just started and a reorg
         happened immediately.  This is just "bad luck" and perfectly fine.
         Second, because a reorg longer than the pruning period happened.
         This is very bad, and will fail later when trying to fetch the
         already-pruned undo data.  */
      LOG (WARNING) << "Trying to detach block from empty pruning queue";
      return;
    }

  VLOG (1) << "Detaching block from pruning queue: " << hashes.back ().ToHex ();
  hashes.pop_back ();
}

} // namespace internal
} // namespace xaya
