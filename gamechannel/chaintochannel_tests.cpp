// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chaintochannel.hpp"

#include "channelmanager_tests.hpp"
#include "database.hpp"
#include "gamestatejson.hpp"
#include "stateproof.hpp"

#include "rpc-stubs/channelgsprpcserverstub.h"

#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>

#include <google/protobuf/text_format.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using google::protobuf::TextFormat;

namespace xaya
{
namespace
{

/** Timeout (in milliseconds) for the test GSP connection.  */
constexpr int RPC_TIMEOUT_MS = 50;

/* ************************************************************************** */

/**
 * Utilities for implementing fake waitforchange calls, including the
 * ability to notify them and wait for a thread to be actually woken
 * and finish processing.  This is useful to ensure that the update we
 * are looking for has indeed happened and we can verify the results.
 */
class NotifiableWaiter
{

private:

  /** Some name for this waiter, used in log messages.  */
  const std::string name;

  std::mutex mut;
  std::condition_variable cv;

  /**
   * If set, allow multiple threads to call Wait at the same time.  This is
   * needed to test what happens when calls time out.
   */
  std::atomic<bool> allowMulti;

  /**
   * Whether to allow calls to Wait to block.  We set this to false in
   * order to enable a clean shutdown.
   */
  std::atomic<bool> block;

  /** Number of threads that are currently in the Wait call.  */
  std::atomic<unsigned> numWaiting;

  /**
   * Number of times Wait has been called.  This can be used to wait until
   * the function returns and is called again after notifying of the update.
   */
  std::atomic<unsigned> waitCalls;

public:

  NotifiableWaiter (const std::string& n)
    : name(n)
  {
    allowMulti = false;
    block = true;
    numWaiting = false;
    waitCalls = 0;
  }

  /**
   * Returns the mutex used internally, so we can lock it for other, external
   * updates that should be synchronised.
   */
  std::mutex&
  GetMutex ()
  {
    return mut;
  }

  /**
   * Allows multiple calls to Wait at the same time.  By default, we do not
   * allow that.  But we need to in order to test timeouts of callers.
   */
  void
  AllowMultiCalls ()
  {
    allowMulti = true;
  }

  /**
   * Disables blocking in Wait calls and wakes up all currently blocked
   * threads.  This ensures that we are ready to stop gracefully.
   */
  void
  StopBlocking ()
  {
    std::lock_guard<std::mutex> lock(mut);
    block = false;
    cv.notify_all ();
  }

  /**
   * Waits for the condition to be notified the next time.
   */
  void
  Wait ()
  {
    if (!block)
      return;

    VLOG (1) << "Preparing to wait for '" << name << "'";
    std::unique_lock<std::mutex> lock(mut);

    if (!allowMulti)
      CHECK_EQ (numWaiting, 0)
          << "There is already a thread waiting for " << name;

    ++waitCalls;
    ++numWaiting;
    VLOG (1) << "Waiting for update to '" << name << "'";
    cv.wait (lock);
    VLOG (1) << "Received update to '" << name << "'";
    --numWaiting;
  }

  /**
   * Performs an update to some state by calling the passed closure,
   * and then notifies the waiting thread of an update.  This ensures that
   * there actually is a waiting thread before performing the callback,
   * and that the waiter has finished processing (i.e. called Wait again)
   * before returning.
   *
   * While the callback is executing, the lock will be held so that it
   * can safely update any state required without racing with the
   * threads calling Wait.
   */
  void
  Update (const std::function<void ()>& cb)
  {
    CHECK (block) << "Waiter is already shutting down!";

    VLOG (1) << "Sleeping until we have a waiting thread on '" << name << "'";
    while (numWaiting == 0)
      SleepSome ();

    unsigned oldCalls;
    {
      std::lock_guard<std::mutex> lock(mut);
      cb ();

      oldCalls = waitCalls;

      LOG (INFO) << "Notifying waiter '" << name << "' about an update";
      cv.notify_all ();
    }

    while (waitCalls == oldCalls)
      SleepSome ();

    if (allowMulti)
      CHECK_GT (waitCalls, oldCalls);
    else
      CHECK_EQ (waitCalls, oldCalls + 1);
  }

};

/* ************************************************************************** */

/**
 * GSP RPC server for use in the tests.  It allows to set a current
 * state that is returned from getchannel, and it allows signalling
 * waitforchange waiters.
 *
 * It knows the test channel's ID and loads data for it from the
 * underlying SQLite database as needed.  So to change the data
 * returned for getchannel, update the database.
 */
class TestGspServer : public ChannelGspRpcServerStub
{

private:

  const uint256& channelId;
  const proto::ChannelMetadata& meta;

  TestGame& game;
  ChannelsTable tbl;

  std::string gspState;
  uint256 bestBlockHash;

  NotifiableWaiter waiterBlocks;

public:

  static constexpr int HTTP_PORT = 32200;
  static constexpr const char* HTTP_URL = "http://localhost:32200";
  using RpcClient = ChannelGspRpcClient;

  explicit TestGspServer (jsonrpc::AbstractServerConnector& conn,
                          const uint256& id, const proto::ChannelMetadata& m,
                          TestGame& g)
    : ChannelGspRpcServerStub(conn), channelId(id), meta(m),
      game(g), tbl(game),
      waiterBlocks("blocks")
  {
    EXPECT_CALL (*this, stop ()).Times (0);
    EXPECT_CALL (*this, getcurrentstate ()).Times (0);
  }

  /**
   * Disables blocking in the waitforchange calls and wakes up
   * all currently blocked threads.  This ensures that we are ready to
   * stop the server and feeder loop.
   */
  void
  StopBlocking ()
  {
    LOG (INFO) << "Disabling blocking in the test GSP server...";
    waiterBlocks.StopBlocking ();
  }

  /**
   * Marks the current state as having no game state in the GSP yet.
   */
  void
  SetNoState (const std::string& state)
  {
    gspState = state;
    bestBlockHash.SetNull ();
  }

  /**
   * Sets the current state to be returned (but does not signal
   * waiting threads).  The block hash is computed by hashing the
   * given string for convenience.
   */
  void
  SetState (const std::string& bestBlkPreimage,
            const std::string& state,
            const BoardState& reinitState,
            const proto::StateProof& proof,
            const unsigned disputeHeight)
  {
    LOG (INFO) << "Setting on-chain state to block: " << bestBlkPreimage;

    gspState = state;
    bestBlockHash = SHA256::Hash (bestBlkPreimage);

    tbl.DeleteById (channelId);

    auto h = tbl.CreateNew (channelId);
    h->Reinitialise (meta, reinitState);
    h->SetStateProof (proof);
    if (disputeHeight != 0)
      h->SetDisputeHeight (disputeHeight);
  }

  /**
   * Sets the current state to be that the test channel does not exist.
   */
  void
  SetChannelNotOnChain (const std::string& bestBlkPreimage,
                        const std::string& state)
  {
    gspState = state;
    bestBlockHash = SHA256::Hash (bestBlkPreimage);

    tbl.DeleteById (channelId);
  }

  /**
   * Returns the NotifiableWaiter instance for block notifications.
   */
  NotifiableWaiter&
  Blocks ()
  {
    return waiterBlocks;
  }

  Json::Value
  getchannel (const std::string& channelIdHex) override
  {
    LOG (INFO) << "RPC call: getchannel " << channelIdHex;
    std::lock_guard<std::mutex> lock(waiterBlocks.GetMutex ());

    Json::Value res(Json::objectValue);
    res["state"] = gspState;
    if (bestBlockHash.IsNull ())
      return res;
    res["blockhash"] = bestBlockHash.ToHex ();
    res["height"] = 42;

    uint256 reqId;
    CHECK (reqId.FromHex (channelIdHex));

    auto h = tbl.GetById (reqId);
    if (h == nullptr)
      res["channel"] = Json::Value ();
    else
      res["channel"] = ChannelToGameStateJson (*h, game.rules);

    return res;
  }

  std::string
  waitforchange (const std::string& knownBlock) override
  {
    LOG (INFO) << "RPC call: waitforchange " << knownBlock;
    waiterBlocks.Wait ();

    if (bestBlockHash.IsNull ())
      return "";

    return bestBlockHash.ToHex ();
  }

  MOCK_METHOD0 (stop, void ());
  MOCK_METHOD0 (getcurrentstate, Json::Value ());
  MOCK_METHOD0 (getpendingstate, Json::Value ());
  MOCK_METHOD1 (waitforpendingchange, Json::Value (int));

};

/* ************************************************************************** */

class ChainToChannelFeederTests : public ChannelManagerTestFixture
{

protected:

  ChainToChannelFeeder feeder;
  HttpRpcServer<TestGspServer> gspServer;

  ChainToChannelFeederTests ()
    : feeder(gspServer.GetClient (), cm),
      gspServer(channelId, meta, game)
  {
    gspServer.GetClientConnector ().SetTimeout (RPC_TIMEOUT_MS);
  }

  ~ChainToChannelFeederTests ()
  {
    /* Shut down the waiting loop gracefully and wake up any waiting
       threads so we can stop.  */
    gspServer->StopBlocking ();
    feeder.Stop ();
  }

  /**
   * Exposes the dispute height to subtests.  Returns 0 if there is no dispute.
   */
  unsigned
  GetDisputeHeight () const
  {
    const auto* disp = GetDispute ();
    if (disp == nullptr)
      return 0;
    return disp->height;
  }

};

/* ************************************************************************** */

using UpdateDataTests = ChainToChannelFeederTests;

TEST_F (UpdateDataTests, NotUpToDate)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetState ("blk", "catching-up", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (UpdateDataTests, NoGspState)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetNoState ("up-to-date");
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (UpdateDataTests, ChannelNotOnChain)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetChannelNotOnChain ("blk", "up-to-date");
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  EXPECT_FALSE (GetExists ());
}

TEST_F (UpdateDataTests, BlockHashAndHeight)
{
  gspServer->SetChannelNotOnChain ("blk 1", "up-to-date");
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  unsigned height;
  uint256 hash = GetOnChainBlock (height);
  EXPECT_EQ (height, 42);
  EXPECT_EQ (hash, SHA256::Hash ("blk 1"));

  gspServer->Blocks ().Update ([this] ()
    {
      gspServer->SetState ("blk 2", "up-to-date",
                           "0 0", ValidProof ("10 5"), 0);
    });

  hash = GetOnChainBlock (height);
  EXPECT_EQ (height, 42);
  EXPECT_EQ (hash, SHA256::Hash ("blk 2"));
}

TEST_F (UpdateDataTests, UpdatesProof)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  EXPECT_EQ (GetLatestState (), "20 6");
}

TEST_F (UpdateDataTests, Reinitialisation)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);

  meta.set_reinit ("other reinit");
  proto::StateProof reinitBasedProof;
  CHECK (TextFormat::ParseFromString (R"(
    initial_state: { data: "42 10" }
    transitions:
      {
        move: "1"
        new_state:
          {
            data: "43 11"
            signatures: "sgn"
          }
      }
  )", &reinitBasedProof));

  gspServer->SetState ("blk", "up-to-date", "42 10", reinitBasedProof, 0);
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  EXPECT_EQ (GetLatestState (), "43 11");
  EXPECT_EQ (GetBoardStates ().GetReinitId (), "other reinit");
}

TEST_F (UpdateDataTests, NoDispute)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  EXPECT_EQ (GetDisputeHeight (), 0);
}

TEST_F (UpdateDataTests, WithDispute)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 42);
  feeder.Start ();

  gspServer->Blocks ().Update ([] () {});
  EXPECT_EQ (GetDisputeHeight (), 42);
}

/* ************************************************************************** */

class WaitForChangeLoopTests : public ChainToChannelFeederTests
{

protected:

  WaitForChangeLoopTests ()
  {
    gspServer->SetState ("start", "up-to-date", "0 0", ValidProof ("0 0"), 0);
    feeder.Start ();
    SleepSome ();
  }

};

TEST_F (WaitForChangeLoopTests, UpdateLoopRuns)
{
  gspServer->Blocks ().Update ([] () {});
  EXPECT_EQ (GetLatestState (), "0 0");

  gspServer->Blocks ().Update ([this] ()
    {
      gspServer->SetState ("blk 1", "up-to-date",
                           "0 0", ValidProof ("10 5"), 0);
    });
  EXPECT_EQ (GetLatestState (), "10 5");

  gspServer->Blocks ().Update ([this] ()
    {
      gspServer->SetState ("blk 2", "up-to-date",
                           "0 0", ValidProof ("20 6"), 0);
    });
  EXPECT_EQ (GetLatestState (), "20 6");
}

TEST_F (WaitForChangeLoopTests, NoGspState)
{
  gspServer->Blocks ().Update ([this] ()
    {
      gspServer->SetNoState ("up-to-date");
    });
  EXPECT_EQ (GetLatestState (), "0 0");
}

TEST_F (WaitForChangeLoopTests, NoChangeInBlock)
{
  gspServer->Blocks ().Update ([this] ()
    {
      gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("10 5"), 0);
    });
  EXPECT_EQ (GetLatestState (), "10 5");

  /* The new state has the same "block hash" (even though it is not the same
     actual state).  Thus the update should be skipped, and the old state
     should remain.  */
  gspServer->Blocks ().Update ([this] ()
    {
      gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
    });
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (WaitForChangeLoopTests, TimeoutsGetRepeated)
{
  gspServer->Blocks ().AllowMultiCalls ();

  gspServer->Blocks ().Update ([this] ()
    {
      gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("10 5"), 0);

      const auto timeout = std::chrono::milliseconds (RPC_TIMEOUT_MS);
      std::this_thread::sleep_for (2 * timeout);
    });

  /* The tracking of when processing has finished does not work well in this
     case, since we have multiple timed out calls that return from Wait without
     doing anything (because the client has already retried and is no longer
     receiving the response at all).  Thus just give us one more chance
     to process the current state.  */
  gspServer->Blocks ().Update ([] () {});

  EXPECT_EQ (GetLatestState (), "10 5");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
