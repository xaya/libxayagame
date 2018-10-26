// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionmanager.hpp"

#include "storage.hpp"

#include "testutils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace internal
{
namespace
{

using testing::InSequence;

/**
 * Simple subclass of MemoryStorage that mocks the transaction methods
 * so we can verify calls to them using gmock.
 */
class MockedStorage : public MemoryStorage
{

public:

  MockedStorage ()
  {
    /* By default, expect no calls to be made.  The calls that we expect
       should explicitly be specified in the individual tests.  */
    EXPECT_CALL (*this, BeginTransaction ()).Times (0);
    EXPECT_CALL (*this, CommitTransaction ()).Times (0);
    EXPECT_CALL (*this, RollbackTransaction ()).Times (0);
  }

  MOCK_METHOD0 (BeginTransaction, void ());
  MOCK_METHOD0 (CommitTransaction, void ());
  MOCK_METHOD0 (RollbackTransaction, void ());

};

class TransactionManagerTests : public testing::Test
{

protected:

  TransactionManager tm;
  MockedStorage storage;

  TransactionManagerTests ()
  {
    /* For ease-of-use in most tests, we already set up the transaction manager
       and storage.  Some tests (e.g. for the destructor) will need a
       different setting, but they can just customly instantiate another
       TransactionManager.  */
    tm.SetStorage (storage);
  }

};

TEST_F (TransactionManagerTests, NoBatching)
{
  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, CommitTransaction ());

    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransaction ());

    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, CommitTransaction ());
  }

  tm.SetBatchSize (1);

  tm.BeginTransaction ();
  tm.CommitTransaction ();

  tm.BeginTransaction ();
  tm.RollbackTransaction ();

  tm.BeginTransaction ();
  tm.CommitTransaction ();
}

TEST_F (TransactionManagerTests, BasicBatching)
{
  {
    InSequence dummy;
    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, CommitTransaction ());
  }

  tm.SetBatchSize (2);

  tm.BeginTransaction ();
  tm.CommitTransaction ();

  tm.BeginTransaction ();
  tm.CommitTransaction ();
}

TEST_F (TransactionManagerTests, Rollback)
{
  {
    InSequence dummy;
    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransaction ());
  }

  tm.SetBatchSize (10);

  tm.BeginTransaction ();
  tm.CommitTransaction ();

  tm.BeginTransaction ();
  tm.RollbackTransaction ();
}

TEST_F (TransactionManagerTests, DestructorTriggersFlush)
{
  {
    InSequence dummy;
    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, CommitTransaction ());
  }

  /* We need to use a custom instance, so that it gets destructed right
     during this test case (not only later as part of the fixture).  */
  TransactionManager m;
  m.SetStorage (storage);

  m.SetBatchSize (10);

  m.BeginTransaction ();
  m.CommitTransaction ();
}

TEST_F (TransactionManagerTests, SetStorageFlushes)
{
  MockedStorage secondStorage;

  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, CommitTransaction ());

    EXPECT_CALL (secondStorage, BeginTransaction ());
    EXPECT_CALL (secondStorage, RollbackTransaction ());
  }

  tm.SetBatchSize (10);
  tm.BeginTransaction ();
  tm.CommitTransaction ();

  /* Changing the storage should flush the previous one.  The next
     transaction (aborted) will be on secondStorage.  */
  tm.SetStorage (secondStorage);
  tm.BeginTransaction ();
  tm.RollbackTransaction ();
}

using SetBatchSizeTests = TransactionManagerTests;

TEST_F (SetBatchSizeTests, TriggersFlush)
{
  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, CommitTransaction ());

    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransaction ());
  }

  tm.SetBatchSize (10);

  tm.BeginTransaction ();
  tm.CommitTransaction ();

  /* Setting the batch size to one triggers a flush.  */
  tm.SetBatchSize (1);

  /* Create a rollback now, which would "cancel" the previous commit in
     case the flush wouldn't have been performed.  */
  tm.BeginTransaction ();
  tm.RollbackTransaction ();
}

TEST_F (SetBatchSizeTests, NoFlushWhenTransactionInProgress)
{
  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransaction ());
  }

  tm.SetBatchSize (10);

  tm.BeginTransaction ();
  tm.CommitTransaction ();
  tm.BeginTransaction ();

  /* Setting the batch size to one will not trigger a flush, since
     we have a started transaction as well.  */
  tm.SetBatchSize (1);

  /* This rollback will cancel also the (not-yet-flushed) previous commit,
     so that no CommitTransaction() should be called at all.  */
  tm.RollbackTransaction ();
}

} // anonymous namespace
} // namespace internal
} // namespace xaya
