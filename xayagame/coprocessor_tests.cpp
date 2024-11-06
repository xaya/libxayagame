// Copyright (C) 2023-2024 The Xaya developers
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

  /** Whether constructed block processors should expect Start() calls.  */
  bool shouldStartBeCalled = true;

  /** Whether constructed block processors should throw in Start().  */
  bool throwInStart = false;

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
   * Specifies that block processors should not expect Start() calls.
   */
  void
  StartShouldNotBeCalled ()
  {
    shouldStartBeCalled = false;
  }

  /**
   * Specifies that block processors should throw in Start().
   */
  void
  ThrowInStart ()
  {
    throwInStart = true;
    shouldStartBeCalled = false;
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

  /** Whether or not the block should be started with Start() at all.  */
  const bool shouldStartBeCalled;

  /** If set to true, then fail (throw) from Start().  */
  const bool throwInStart;

  /** Whether or not Start() has been called.  */
  bool startCalled = false;
  /** Whether or not Finish() has been called.  */
  bool finishCalled = false;
  /** Whether or not Abort() has been called.  */
  bool abortCalled = false;

protected:

  void
  Start () override
  {
    /* In case of throwInStart, we actually have shouldStartBeCalled set
       to false (even though we expect this call), since then we do not
       expect a Finish() for this instance.  Thus throw
       here before doing any of the other things.  */
    if (throwInStart)
      throw std::runtime_error ("Start() error");

    EXPECT_TRUE (shouldStartBeCalled) << "Unexpected Start() call";
    EXPECT_FALSE (startCalled) << "Start() called twice";
    startCalled = true;
  }

  void
  Finish () override
  {
    EXPECT_TRUE (startCalled) << "Resolution without Start()";
    EXPECT_FALSE (finishCalled) << "Duplicate resolution call";
    EXPECT_TRUE (shouldBeSuccess) << "Expected failure, but Commit() called";
    finishCalled = true;
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
      shouldBeSuccess(s), shouldStartBeCalled(b), throwInStart(t),
      parent(p)
  {}

  ~MockBlock ()
  {
    if (shouldStartBeCalled)
      {
        EXPECT_TRUE (startCalled) << "Start() has not been called";
        /* The call to Finish() is not guaranteed.  */
      }
    /* Otherwise, we already fail the test in Start() being called, or
       when Finish() is called without Start().  */
  }

};

std::unique_ptr<Coprocessor::Block>
MockCoprocessor::ForBlock (const Json::Value& blockData, const Op op)
{
  return std::make_unique<MockBlock> (blockData, op,
                                      shouldBeSuccess, shouldStartBeCalled,
                                      throwInStart, *this);
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
    batchBlock.Start ();
    batchBlock.Finish ();
  }

  {
    CoprocessorBatch::Block batchBlock(batch, blockData,
                                       Coprocessor::Op::BACKWARD);
    EXPECT_EQ (GetMockBlock (batchBlock).GetOperation (),
               Coprocessor::Op::BACKWARD);
    batchBlock.Start ();
    batchBlock.Finish ();
  }
}

TEST_F (CoprocessorTests, ErrorInStart)
{
  batch.Add ("mock2", proc2);
  batch.Add ("mock3", proc3);

  coproc.ExpectFailure ();
  proc2.ThrowInStart ();
  proc3.StartShouldNotBeCalled ();

  EXPECT_THROW (
    {
      CoprocessorBatch::Block batchBlock(batch, FakeBlockData (10),
                                         Coprocessor::Op::FORWARD);
      batchBlock.Start ();
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

  batchBlock.Start ();
  batchBlock.Finish ();
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
