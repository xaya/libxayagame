// Copyright (C) 2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coprocessor.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>

namespace xaya
{

/* ************************************************************************** */

namespace
{

/**
 * A mock Coprocessor that verifies that exactly the expected of
 * Commitor Abort is called on each Block subprocessor.
 */
class MockCoprocessor : public Coprocessor
{

private:

  /** Whether constructed block processors should expect success.  */
  bool shouldBeSuccess = true;

  /** Whether constructed block processors should expect Begin() calls.  */
  bool shouldBeginBeCalled = true;

  /** Whether constructed block processors should throw in Begin().  */
  bool throwInBegin = false;

public:

  class MockBlock;

  /**
   * Specifies that constructed block processors should be failures.
   */
  void
  ExpectFailure ()
  {
    shouldBeSuccess = false;
  }

  /**
   * Specifies that block processors should not expect Begin() calls.
   */
  void
  BeginShouldNotBeCalled ()
  {
    shouldBeginBeCalled = false;
  }

  /**
   * Specifies that block processors should throw in Begin().
   */
  void
  ThrowInBegin ()
  {
    throwInBegin = true;
    shouldBeginBeCalled = false;
  }

  std::unique_ptr<Block> ForBlock (const Json::Value& blockData,
                                   Op op) override;

};

class MockCoprocessor::MockBlock : public Coprocessor::Block
{

private:

  /**
   * Whether the block should be handled as successful, i.e. Commit()
   * should be called.  If false, then Abort() must be called.
   */
  const bool shouldBeSuccess;

  /** Whether or not the block should be started with Begin() at all.  */
  const bool shouldBeginBeCalled;

  /** If set to true, then fail (throw) from Begin().  */
  const bool throwInBegin;

  /** Whether or not Begin() has been called.  */
  bool beginCalled = false;
  /** Whether or not Commit() has been called.  */
  bool commitCalled = false;
  /** Whether or not Abort() has been called.  */
  bool abortCalled = false;

protected:

  void
  Begin () override
  {
    /* In case of throwInBegin, we actually have shouldBeginBeCalled set
       to false (even though we expect this call), since then we do not
       expect a Commit() or Abort() for this instance.  Thus throw
       here before doing any of the other things.  */
    if (throwInBegin)
      throw std::runtime_error ("Begin() error");

    EXPECT_TRUE (shouldBeginBeCalled) << "Unexpected Begin() call";
    EXPECT_FALSE (beginCalled) << "Begin() called twice";
    beginCalled = true;
  }

  void
  Commit () override
  {
    EXPECT_TRUE (beginCalled) << "Resolution without Begin()";
    EXPECT_FALSE (commitCalled || abortCalled) << "Duplicate resolution call";
    EXPECT_TRUE (shouldBeSuccess) << "Expected failure, but Commit() called";
    commitCalled = true;
  }

  void
  Abort () override
  {
    EXPECT_TRUE (beginCalled) << "Resolution without Begin()";
    EXPECT_FALSE (commitCalled || abortCalled) << "Duplicate resolution call";
    EXPECT_FALSE (shouldBeSuccess) << "Expected success, but Abort() called";
    abortCalled = true;
  }

public:

  /**
   * The parent coprocessor.  This is used in tests to verify that the
   * block requested from a batch by name is the correct one (i.e. matching
   * the coprocessor of this name).
   */
  const MockCoprocessor& parent;

  MockBlock (const Json::Value& blockData, const Coprocessor::Op o,
             const bool s, const bool b, const bool t,
             const MockCoprocessor& p)
    : Block(blockData, o),
      shouldBeSuccess(s), shouldBeginBeCalled(b), throwInBegin(t),
      parent(p)
  {}

  ~MockBlock ()
  {
    if (shouldBeginBeCalled)
      {
        /* If both have been called, then we already failed the test
           in the Commit() or Abort() method.  */
        EXPECT_TRUE (beginCalled) << "Begin() has not been called";
        EXPECT_TRUE (commitCalled || abortCalled) << "No resolution call";
      }
    /* Otherwise, we already fail the test in Begin() being called, or
       when Commit() or Abort() are called without Begin().  */
  }

};

std::unique_ptr<Coprocessor::Block>
MockCoprocessor::ForBlock (const Json::Value& blockData, const Op op)
{
  return std::make_unique<MockBlock> (blockData, op,
                                      shouldBeSuccess, shouldBeginBeCalled,
                                      throwInBegin, *this);
}

/* ************************************************************************** */

} // anonymous namespace

class CoprocessorTests : public testing::Test
{

protected:

  MockCoprocessor coproc;
  MockCoprocessor proc2;
  MockCoprocessor proc3;

  CoprocessorBatch batch;

  CoprocessorTests ()
  {
    batch.Add ("mock", coproc);
  }

  /**
   * Returns the MockBlock of the given coprocessor batch.
   */
  MockCoprocessor::MockBlock&
  GetMockBlock (CoprocessorBatch::Block& blk)
  {
    auto* res = blk.Get<MockCoprocessor::MockBlock> ("mock");
    CHECK (res != nullptr);
    return *res;
  }

  /**
   * Constructs fake block data for the given block height.
   */
  static Json::Value
  FakeBlockData (const unsigned height)
  {
    Json::Value res(Json::objectValue);
    res["hash"] = BlockHash (height).ToHex ();
    res["height"] = static_cast<Json::Int64> (height);

    return res;
  }

};

namespace
{

TEST_F (CoprocessorTests, ParsesBlockData)
{
  const auto blockData = FakeBlockData (123);

  {
    CoprocessorBatch::Block batchBlock(batch, blockData,
                                       Coprocessor::Op::FORWARD);
    auto& blk = GetMockBlock (batchBlock);
    EXPECT_EQ (blk.GetBlockData (), blockData);
    EXPECT_EQ (blk.GetBlockHash (), BlockHash (123));
    EXPECT_EQ (blk.GetBlockHeight (), 123);
    EXPECT_EQ (blk.GetOperation (), Coprocessor::Op::FORWARD);
    batchBlock.Begin ();
    batchBlock.Commit ();
  }

  {
    CoprocessorBatch::Block batchBlock(batch, blockData,
                                       Coprocessor::Op::BACKWARD);
    EXPECT_EQ (GetMockBlock (batchBlock).GetOperation (),
               Coprocessor::Op::BACKWARD);
    batchBlock.Begin ();
    batchBlock.Commit ();
  }
}

TEST_F (CoprocessorTests, CommitAndAbort)
{
  {
    CoprocessorBatch::Block batchBlock(batch, FakeBlockData (10),
                                       Coprocessor::Op::FORWARD);
    /* By default, the mock block expects success.  */
    batchBlock.Begin ();
    batchBlock.Commit ();
  }

  {
    coproc.ExpectFailure ();
    CoprocessorBatch::Block batchBlock(batch, FakeBlockData (11),
                                       Coprocessor::Op::FORWARD);
    batchBlock.Begin ();
    /* We do not commit, which means that it should be aborted instead.  */
  }
}

TEST_F (CoprocessorTests, ErrorInBegin)
{
  batch.Add ("mock2", proc2);
  batch.Add ("mock3", proc3);

  coproc.ExpectFailure ();
  proc2.ThrowInBegin ();
  proc3.BeginShouldNotBeCalled ();

  EXPECT_THROW (
    {
      CoprocessorBatch::Block batchBlock(batch, FakeBlockData (10),
                                         Coprocessor::Op::FORWARD);
      batchBlock.Begin ();
    },
    std::runtime_error
  );
}

TEST_F (CoprocessorTests, ByName)
{
  batch.Add ("2", proc2);
  CoprocessorBatch::Block batchBlock(batch, FakeBlockData (10),
                                     Coprocessor::Op::FORWARD);

  EXPECT_EQ (batchBlock.Get<MockCoprocessor::MockBlock> ("foo"), nullptr);
  EXPECT_EQ (&batchBlock.Get<MockCoprocessor::MockBlock> ("mock")->parent,
             &coproc);
  EXPECT_EQ (&batchBlock.Get<MockCoprocessor::MockBlock> ("2")->parent,
             &proc2);

  batchBlock.Begin ();
  batchBlock.Commit ();
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
