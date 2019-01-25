// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include <glog/logging.h>

#include <chrono>
#include <cstdio>
#include <thread>

namespace xaya
{

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
