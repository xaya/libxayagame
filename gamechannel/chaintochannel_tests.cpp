// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chaintochannel.hpp"

#include "database.hpp"
#include "gamestatejson.hpp"
#include "stateproof.hpp"
#include "testgame.hpp"

#include "rpc-stubs/channelgsprpcserverstub.h"

#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

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
 * Constructs a state proof for the given state, signed by both players
 * (and thus valid).
 */
proto::StateProof
ValidProof (const std::string& state)
{
  proto::StateProof res;
  auto* is = res.mutable_initial_state ();
  is->set_data (state);
  is->add_signatures ("sgn");
  is->add_signatures ("other sgn");

  return res;
}

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

  explicit TestGspServer (const uint256& id, const proto::ChannelMetadata& m,
                          TestGame& g,
                          jsonrpc::AbstractServerConnector& conn)
    : ChannelGspRpcServerStub(conn), channelId(id), meta(m),
      game(g), tbl(game)
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

};

} // anonymous namespace

class ChainToChannelFeederTests : public TestGameFixture
{

private:

  const uint256 channelId = SHA256::Hash ("channel id");

  jsonrpc::HttpServer httpServerGsp;
  jsonrpc::HttpClient httpClientGsp;

protected:

  proto::ChannelMetadata meta;

  ChannelManager cm;
  ChainToChannelFeeder feeder;
  TestGspServer gspServer;

  ChainToChannelFeederTests ()
    : httpServerGsp(32200),
      httpClientGsp("http://localhost:32200"),
      cm(game.rules, rpcClient, rpcWallet, channelId, "player"),
      feeder(httpClientGsp, cm),
      gspServer(channelId, meta, game, httpServerGsp)
  {
    httpClientGsp.SetTimeout (RPC_TIMEOUT_MS);

    CHECK (TextFormat::ParseFromString (R"(
      reinit: "reinit id"
      participants:
        {
          name: "player"
          address: "my addr"
        }
      participants:
        {
          name: "other"
          address: "not my addr"
        }
    )", &meta));

    ValidSignature ("sgn", "my addr");
    ValidSignature ("other sgn", "not my addr");

    gspServer.StartListening ();
  }

  ~ChainToChannelFeederTests ()
  {
    /* Shut down the waiting loop gracefully and wake up any waiting
       threads so we can stop.  */
    feeder.Stop ();
    gspServer.NotifyChange ();

    cm.StopUpdates ();
    gspServer.StopListening ();
  }

  /**
   * Extracts the latest state from boardStates.
   */
  BoardState
  GetLatestState () const
  {
    return UnverifiedProofEndState (cm.boardStates.GetStateProof ());
  }

  /**
   * Exposes the boardStates member of our ChannelManager to subtests.
   */
  const RollingState&
  GetBoardStates () const
  {
    return cm.boardStates;
  }

  /**
   * Exposes the exists member to subtests.
   */
  bool
  GetExists () const
  {
    return cm.exists;
  }

  /**
   * Exposes the dispute height to subtests.  Returns 0 if there is no dispute.
   */
  unsigned
  GetDisputeHeight () const
  {
    if (cm.dispute == nullptr)
      return 0;
    return cm.dispute->height;
  }

};

namespace
{

/* ************************************************************************** */

using UpdateDataTests = ChainToChannelFeederTests;

TEST_F (UpdateDataTests, NotUpToDate)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  gspServer.SetState ("blk", "catching-up", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (UpdateDataTests, NoGspState)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  gspServer.SetNoState ("up-to-date");
  feeder.Start ();

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (UpdateDataTests, ChannelNotOnChain)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  gspServer.SetChannelNotOnChain ("blk", "up-to-date");
  feeder.Start ();

  SleepSome ();
  EXPECT_FALSE (GetExists ());
}

TEST_F (UpdateDataTests, UpdatesProof)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  gspServer.SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "20 6");
}

TEST_F (UpdateDataTests, Reinitialisation)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);

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

  gspServer.SetState ("blk", "up-to-date", "42 10", reinitBasedProof, 0);
  feeder.Start ();

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "43 11");
  EXPECT_EQ (GetBoardStates ().GetReinitId (), "other reinit");
}

TEST_F (UpdateDataTests, NoDispute)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  gspServer.SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  feeder.Start ();

  SleepSome ();
  EXPECT_EQ (GetDisputeHeight (), 0);
}

TEST_F (UpdateDataTests, WithDispute)
{
  cm.ProcessOnChain (meta, "0 0", ValidProof ("10 5"), 0);
  gspServer.SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 42);
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
    gspServer.SetState ("start", "up-to-date", "0 0", ValidProof ("0 0"), 0);
    feeder.Start ();
    SleepSome ();
  }

};

TEST_F (WaitForChangeLoopTests, UpdateLoopRuns)
{
  gspServer.SetState ("blk 1", "up-to-date", "0 0", ValidProof ("10 5"), 0);

  SleepSome ();
  EXPECT_EQ (GetLatestState (), "0 0");

  gspServer.NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");

  gspServer.SetState ("blk 2", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  gspServer.NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "20 6");
}

TEST_F (WaitForChangeLoopTests, NoGspState)
{
  gspServer.SetNoState ("up-to-date");
  gspServer.NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "0 0");
}

TEST_F (WaitForChangeLoopTests, NoChangeInBlock)
{
  gspServer.SetState ("blk", "up-to-date", "0 0", ValidProof ("10 5"), 0);
  gspServer.NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");

  gspServer.SetState ("blk", "up-to-date", "0 0", ValidProof ("20 6"), 0);
  gspServer.NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

TEST_F (WaitForChangeLoopTests, TimeoutsGetRepeated)
{
  gspServer.SetState ("blk", "up-to-date", "0 0", ValidProof ("10 5"), 0);

  std::this_thread::sleep_for (std::chrono::milliseconds (2 * RPC_TIMEOUT_MS));
  EXPECT_EQ (GetLatestState (), "0 0");

  gspServer.NotifyChange ();
  SleepSome ();
  EXPECT_EQ (GetLatestState (), "10 5");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya