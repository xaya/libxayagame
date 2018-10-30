// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_TRANSACTIONMANAGER_HPP
#define XAYAGAME_TRANSACTIONMANAGER_HPP

/* This file is an implementation detail of Game and should not be
   used directly by external code!  */

#include "storage.hpp"

namespace xaya
{
namespace internal
{

/**
 * Utility class that takes care of (potentially) batching together
 * atomic transactions while the game is catching up.  It has an underlying
 * storage interface, on which transaction handling is done.  But it also
 * allows to enable batching, in which case a started transaction will not
 * immediately be committed, but only after the manager has been requested
 * to do a certain number of transactions itself.
 */
class TransactionManager
{

private:

  /** The underlying storage instance.  */
  StorageInterface* storage = nullptr;

  /** The desired batch size.  <= 1 means batching is disabled.  */
  unsigned batchSize = 1;

  /**
   * Number of already "committed" but batched transactions.  If this is
   * nonzero, then a transaction on the underlying storage instance has
   * been started but not yet finished.
   */
  unsigned batchedCommits = 0;

  /**
   * Whether or not a transaction has currently been started *on the manager*.
   * This is independent of batching.
   */
  bool inTransaction = false;

  /**
   * Flushes the current batch of transactions to the underlying storage.
   * This must not be called if a transaction is in progress.
   */
  void Flush ();

public:

  TransactionManager () = default;
  ~TransactionManager ();

  TransactionManager (const TransactionManager&) = delete;
  void operator= (const TransactionManager&) = delete;

  /**
   * Sets the underlying storage instance.  This must not be called while
   * a transaction on the manager is ongoing.  Committed but batched
   * transactions will be committed on the current instance before updating
   * the instance reference.
   */
  void SetStorage (StorageInterface& s);

  /**
   * Changes the desired batch size.  The value must be at least one.  Setting
   * it to one disables batching, a larger number enables batching.  If the
   * number is set to something lower than the number of currently batched
   * transactions, then the batch may be committed right away.
   */
  void SetBatchSize (unsigned sz);

  /**
   * Starts a new transaction on the manager.  Depending on batching
   * behaviour, this may or may not start a transaction on the underlying
   * storage itself.
   */
  void BeginTransaction ();

  /**
   * Commits the currently ongoing transaction on the manager.  This may
   * commit a transaction on the underlying storage, or may just mark the
   * current one as "committed" in the batch and wait for more transactions
   * before committing the entire batch.
   */
  void CommitTransaction ();

  /**
   * Aborts and rolls back the current transaction in the manager.  This has the
   * effect of rolling back the entire current batch in case more transactions
   * have been batched.
   */
  void RollbackTransaction ();

  /**
   * Aborts the current transaction in the backing storage if there is one
   * open.  This makes sure that afterwards there is no open transaction
   * either in the manager or the underlying storage.
   */
  void TryAbortTransaction ();

};

/**
 * Helper class that starts a transaction and either commits or aborts it
 * later based on RAII semantics.
 */
class ActiveTransaction
{

private:

  /** The manager on which to call the functions.  */
  TransactionManager& manager;

  /**
   * Whether the operation was successful.  If this is set to true at some
   * point in time, then CommitTransaction will be called.  Otherwise, the
   * transaction is aborted in the destructor.
   */
  bool success = false;

public:

  explicit ActiveTransaction (TransactionManager& m);
  ~ActiveTransaction ();

  void
  SetSuccess ()
  {
    success = true;
  }

};

} // namespace internal
} // namespace xaya

#endif // XAYAGAME_TRANSACTIONMANAGER_HPP
