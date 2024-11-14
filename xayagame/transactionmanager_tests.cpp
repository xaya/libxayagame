// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionmanager.hpp"

#include "storage.hpp"

#include "testutils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <stdexcept>

namespace xaya
{
namespace internal
{
namespace
{

using testing::InSequence;

class MockedStorage : public TxMockedMemoryStorage
{

public:

  MockedStorage ()
  {
    /* By default, expect no calls to be made.  The calls that we expect
       should explicitly be specified in the individual tests.  */
    EXPECT_CALL (*this, BeginTransactionMock ()).Times (0);
    EXPECT_CALL (*this, CommitTransactionMock ()).Times (0);
    EXPECT_CALL (*this, RollbackTransactionMock ()).Times (0);
  }

};

class MockCoProc : public Coprocessor
{

public:

  MockCoProc ()
  {
    EXPECT_CALL (*this, BeginTransaction ()).Times (0);
    EXPECT_CALL (*this, CommitTransaction ()).Times (0);
    EXPECT_CALL (*this, AbortTransaction ()).Times (0);
  }

  MOCK_METHOD0 (BeginTransaction, void ());
  MOCK_METHOD0 (CommitTransaction, void ());
  MOCK_METHOD0 (AbortTransaction, void ());

  MOCK_METHOD2 (ForBlock, std::unique_ptr<Block> (const Json::Value&, Op));

};

class TransactionManagerTests : public testing::Test
{

protected:

  TransactionManager tm;
  MockedStorage storage;

  MockCoProc coproc;
  CoprocessorBatch cobatch;

  TransactionManagerTests ()
  {
    /* For ease-of-use in most tests, we already set up the transaction manager
       and storage.  Some tests (e.g. for the destructor) will need a
       different setting, but they can just customly instantiate another
       TransactionManager.  */
    tm.SetStorage (storage);
    cobatch.Add ("mock", coproc);
    tm.SetCoprocessor (cobatch);
  }

};

TEST_F (TransactionManagerTests, NoBatching)
{
  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (coproc, CommitTransaction ());
    EXPECT_CALL (storage, CommitTransactionMock ());

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransactionMock ());
    EXPECT_CALL (coproc, AbortTransaction ());

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (coproc, CommitTransaction ());
    EXPECT_CALL (storage, CommitTransactionMock ());
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
    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (coproc, CommitTransaction ());
    EXPECT_CALL (storage, CommitTransactionMock ());
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
    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransactionMock ());
    EXPECT_CALL (coproc, AbortTransaction ());
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
    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (storage, CommitTransactionMock ());
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

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (coproc, CommitTransaction ());
    EXPECT_CALL (storage, CommitTransactionMock ());

    EXPECT_CALL (secondStorage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (secondStorage, RollbackTransactionMock ());
    EXPECT_CALL (coproc, AbortTransaction ());
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

/**
 * Storage instance that throws an exception when committing.
 */
class CommittingFailsStorage : public MockedStorage
{

public:

  class Failure : public std::runtime_error
  {

  public:

    Failure ()
      : std::runtime_error("commit failed")
    {}

  };

  void
  CommitTransaction () override
  {
    CommitTransactionMock ();
    throw Failure ();
  }

};

TEST_F (TransactionManagerTests, CommitFails)
{
  CommittingFailsStorage fallibleStorage;

  {
    InSequence dummy;
    EXPECT_CALL (fallibleStorage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    /* The coprocessors are committed before the storage, so they will commit
       fine and only the storage commit later will fail (and trigger a rollback
       of the storage transaction).  */
    EXPECT_CALL (coproc, CommitTransaction ());
    EXPECT_CALL (fallibleStorage, CommitTransactionMock ());
    EXPECT_CALL (fallibleStorage, RollbackTransactionMock ());
  }

  tm.SetStorage (fallibleStorage);

  try
    {
      ActiveTransaction tx(tm);
      tx.Commit ();
      FAIL () << "Expected exception was not thrown";
    }
  catch (const CommittingFailsStorage::Failure& exc)
    {
      LOG (INFO) << "Caught expected commit failure";
    }
}

using TryAbortTransactionTests = TransactionManagerTests;

TEST_F (TryAbortTransactionTests, NoActiveTransaction)
{
  tm.TryAbortTransaction ();

  /* Calling clear on the storage verifies that we don't have an active
     transaction in it at the moment.  */
  storage.Clear ();
}

TEST_F (TryAbortTransactionTests, BatchedCommits)
{
  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransactionMock ());
    EXPECT_CALL (coproc, AbortTransaction ());
  }

  tm.SetBatchSize (10);

  tm.BeginTransaction ();
  tm.CommitTransaction ();

  tm.TryAbortTransaction ();
  storage.Clear ();
}

TEST_F (TryAbortTransactionTests, ActiveTransaction)
{
  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransactionMock ());
    EXPECT_CALL (coproc, AbortTransaction ());
  }

  tm.BeginTransaction ();
  tm.TryAbortTransaction ();
  storage.Clear ();
}

using SetBatchSizeTests = TransactionManagerTests;

TEST_F (SetBatchSizeTests, TriggersFlush)
{
  {
    InSequence dummy;

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (coproc, CommitTransaction ());
    EXPECT_CALL (storage, CommitTransactionMock ());

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransactionMock ());
    EXPECT_CALL (coproc, AbortTransaction ());
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

    EXPECT_CALL (storage, BeginTransactionMock ());
    EXPECT_CALL (coproc, BeginTransaction ());
    EXPECT_CALL (storage, RollbackTransactionMock ());
    EXPECT_CALL (coproc, AbortTransaction ());
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
