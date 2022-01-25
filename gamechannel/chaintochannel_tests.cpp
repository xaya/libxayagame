// Copyright (C) 2019-2022 The Xaya developers
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

  /* Mutex and condition variable for the faked waitforchange.  */
  std::mutex mut;
  std::condition_variable cv;

public:

  static constexpr int HTTP_PORT = 32200;
  static constexpr const char* HTTP_URL = "http://localhost:32200";
  using RpcClient = ChannelGspRpcClient;

  explicit TestGspServer (jsonrpc::AbstractServerConnector& conn,
                          const uint256& id, const proto::ChannelMetadata& m,
                          SQLiteDatabase& db, TestGame& g)
    : ChannelGspRpcServerStub(conn), channelId(id), meta(m),
      game(g), tbl(db)
  {
    EXPECT_CALL (*this, stop ()).Times (0);
    EXPECT_CALL (*this, getcurrentstate ()).Times (0);
  }

  /**
   * Marks the current state as having no game state in the GSP yet.
   */
  void
  SetNoState (const std::string& state)
  {
    std::lock_guard<std::mutex> lock(mut);
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
    std::lock_guard<std::mutex> lock(mut);

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
    std::lock_guard<std::mutex> lock(mut);

    gspState = state;
    bestBlockHash = SHA256::Hash (bestBlkPreimage);

    tbl.DeleteById (channelId);
  }

  /**
   * Notifies all waiting threads of a change.
   */
  void
  NotifyChange ()
  {
    std::lock_guard<std::mutex> lock(mut);
    cv.notify_all ();
  }

  Json::Value
  getchannel (const std::string& channelIdHex) override
  {
    LOG (INFO) << "RPC call: getchannel " << channelIdHex;
    std::lock_guard<std::mutex> lock(mut);

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
    std::unique_lock<std::mutex> lock(mut);

    cv.wait (lock);

    if (bestBlockHash.IsNull ())
      return "";

    return bestBlockHash.ToHex ();
  }

  MOCK_METHOD0 (stop, void ());
  MOCK_METHOD0 (getcurrentstate, Json::Value ());
  MOCK_METHOD0 (getnullstate, Json::Value ());
  MOCK_METHOD0 (getpendingstate, Json::Value ());
  MOCK_METHOD1 (waitforpendingchange, Json::Value (int));

};

class ChainToChannelFeederTests : public ChannelManagerTestFixture
{

protected:

  /**
   * The Synchronised manager is needed to instantiate the chain-to-channel
   * feeder for the test.  In the actual tests, the feeder is not running
   * at the same time as the main thread is doing changes to the channel
   * manager, though, so we don't actually have to use its lock in the tests.
   */
  SynchronisedChannelManager scm;

  ChainToChannelFeeder feeder;
  HttpRpcServer<TestGspServer> gspServer;

  ChainToChannelFeederTests ()
    : scm(cm), feeder(gspServer.GetClient (), scm),
      gspServer(channelId, meta, GetDb (), game)
  {
    gspServer.GetClientConnector ().SetTimeout (RPC_TIMEOUT_MS);
  }

  ~ChainToChannelFeederTests ()
  {
    /* Shut down the waiting loop gracefully and wake up any waiting
       threads so we can stop.  */
    feeder.Stop ();
    gspServer->NotifyChange ();
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

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (UpdateDataTests, NoGspState)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetNoState ("up-to-date");
  feeder.Start ();

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (UpdateDataTests, ChannelNotOnChain)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetChannelNotOnChain ("blk", "up-to-date");
  feeder.Start ();

  SleepSome ();
  EXPECT_FALSE (GetExists ());
}

TEST_F (UpdateDataTests, BlockHashAndHeight)
{
  gspServer->SetChannelNotOnChain ("blk 1", "up-to-date");
  feeder.Start ();

  SleepSome ();
  unsigned height;
  uint256 hash = GetOnChainBlock (height);
  EXPECT_EQ (height, 42);
  EXPECT_EQ (hash, SHA256::Hash ("blk 1"));

  gspServer->SetState ("blk 2", "up-to-date", "0 0", ValidProof ("10 5"), 0);
  gspServer->NotifyChange ();

  SleepSome ();
  hash = GetOnChainBlock (height);
  EXPECT_EQ (height, 42);
  EXPECT_EQ (hash, SHA256::Hash ("blk 2"));
}

TEST_F (UpdateDataTests, UpdatesProof)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  SleepSome ();
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

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "43 11");
  EXPECT_EQ (GetBoardStates ().GetReinitId (), "other reinit");
}

TEST_F (UpdateDataTests, NoDispute)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  SleepSome ();
  EXPECT_EQ (GetDisputeHeight (), 0);
}

TEST_F (UpdateDataTests, WithDispute)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 42);
  feeder.Start ();

  SleepSome ();
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
  gspServer->SetState ("blk 1", "up-to-date", "0 0", ValidProof ("10 5"), 0);

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "0 0");

  gspServer->NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");

  gspServer->SetState ("blk 2", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  gspServer->NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "20 6");
}

TEST_F (WaitForChangeLoopTests, NoGspState)
{
  gspServer->SetNoState ("up-to-date");
  gspServer->NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "0 0");
}

TEST_F (WaitForChangeLoopTests, NoChangeInBlock)
{
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("10 5"), 0);
  gspServer->NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");

  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  gspServer->NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (WaitForChangeLoopTests, TimeoutsGetRepeated)
{
  gspServer->SetState ("blk", "up-to-date", "0 0", ValidProof ("10 5"), 0);

  std::this_thread::sleep_for (std::chrono::milliseconds (2 * RPC_TIMEOUT_MS));
  EXPECT_EQ (GetLatestState (), "0 0");

  gspServer->NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
