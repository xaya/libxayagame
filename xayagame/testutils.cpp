// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include <glog/logging.h>

#include <chrono>
#include <cstdio>
#include <sstream>
#include <thread>

namespace xaya
{

using testing::_;

uint256
BlockHash (unsigned num)
{
  std::string hex = "ab" + std::string (62, '0');

  CHECK (num < 0x100);
  std::sprintf (&hex[2], "%02x", num);
  CHECK (hex[4] == 0);
  hex[4] = '0';

  uint256 res;
  CHECK (res.FromHex (hex));
  return res;
}

void
SleepSome ()
{
  std::this_thread::sleep_for (std::chrono::milliseconds (10));
}

Json::Value
ParseJson (const std::string& str)
{
  Json::Value val;
  std::istringstream in(str);
  in >> val;
  return val;
}

MockXayaRpcServer::MockXayaRpcServer (jsonrpc::AbstractServerConnector& conn)
  : XayaRpcServerStub(conn)
{
  EXPECT_CALL (*this, getzmqnotifications ()).Times (0);
  EXPECT_CALL (*this, trackedgames (_, _)).Times (0);
  EXPECT_CALL (*this, getnetworkinfo ()).Times (0);
  EXPECT_CALL (*this, getblockchaininfo ()).Times (0);
  EXPECT_CALL (*this, getblockhash (_)).Times (0);
  EXPECT_CALL (*this, getblockheader (_)).Times (0);
  EXPECT_CALL (*this, game_sendupdates (_, _)).Times (0);
  EXPECT_CALL (*this, verifymessage (_, _, _)).Times (0);
  EXPECT_CALL (*this, getrawmempool ()).Times (0);
}

MockXayaWalletRpcServer::MockXayaWalletRpcServer (
    jsonrpc::AbstractServerConnector& conn)
  : XayaWalletRpcServerStub(conn)
{
  EXPECT_CALL (*this, getaddressinfo (_)).Times (0);
  EXPECT_CALL (*this, signmessage (_, _)).Times (0);
  EXPECT_CALL (*this, name_update (_, _)).Times (0);
}

void
GameTestFixture::CallBlockAttach (Game& g, const std::string& reqToken,
                                  const uint256& parentHash,
                                  const uint256& blockHash,
                                  const unsigned height,
                                  const Json::Value& moves,
                                  const bool seqMismatch) const
{
  Json::Value block(Json::objectValue);
  block["hash"] = blockHash.ToHex ();
  block["parent"] = parentHash.ToHex ();
  block["height"] = height;
  block["rngseed"] = blockHash.ToHex ();

  Json::Value data(Json::objectValue);
  if (!reqToken.empty ())
    data["reqtoken"] = reqToken;
  data["block"] = block;
  data["moves"] = moves;

  g.BlockAttach (gameId, data, seqMismatch);
}

void
GameTestFixture::CallBlockDetach (Game& g, const std::string& reqToken,
                                  const uint256& parentHash,
                                  const uint256& blockHash,
                                  const unsigned height,
                                  const Json::Value& moves,
                                  const bool seqMismatch) const
{
  Json::Value block(Json::objectValue);
  block["hash"] = blockHash.ToHex ();
  block["parent"] = parentHash.ToHex ();
  block["height"] = height;
  block["rngseed"] = blockHash.ToHex ();

  Json::Value data(Json::objectValue);
  if (!reqToken.empty ())
    data["reqtoken"] = reqToken;
  data["block"] = block;
  data["moves"] = moves;

  g.BlockDetach (gameId, data, seqMismatch);
}

void
GameTestFixture::CallPendingMove (Game& g, const Json::Value& mv) const
{
  g.PendingMove (gameId, mv);
}

void
GameTestWithBlockchain::SetStartingBlock (const uint256& hash)
{
  blockHashes = {hash};
  moveStack.clear ();
}

void
GameTestWithBlockchain::AttachBlock (Game& g, const uint256& hash,
                                     const Json::Value& moves)
{
  CHECK (!blockHashes.empty ()) << "No starting block has been set";
  CallBlockAttach (g, "",
                   blockHashes.back (), hash, blockHashes.size () + 1,
                   moves, false);
  blockHashes.push_back (hash);
  moveStack.push_back (moves);
}

void
GameTestWithBlockchain::DetachBlock (Game& g)
{
  CHECK (!blockHashes.empty ());
  CHECK (!moveStack.empty ());

  const uint256 hash = blockHashes.back ();
  blockHashes.pop_back ();
  CallBlockDetach (g, "",
                   blockHashes.back (), hash, blockHashes.size () + 1,
                   moveStack.back (), false);
  moveStack.pop_back ();
}

} // namespace xaya
