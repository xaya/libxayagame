// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include <glog/logging.h>

#include <cstdio>

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
GameTestFixture::CallBlockAttach (Game& g, const std::string& reqToken,
                                  const uint256& parentHash,
                                  const uint256& blockHash,
                                  const Json::Value& moves,
                                  const bool seqMismatch) const
{
  Json::Value block(Json::objectValue);
  block["hash"] = blockHash.ToHex ();
  block["parent"] = parentHash.ToHex ();

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
                                  const Json::Value& moves,
                                  const bool seqMismatch) const
{
  Json::Value block(Json::objectValue);
  block["hash"] = blockHash.ToHex ();
  block["parent"] = parentHash.ToHex ();

  Json::Value data(Json::objectValue);
  if (!reqToken.empty ())
    data["reqtoken"] = reqToken;
  data["block"] = block;
  data["moves"] = moves;

  g.BlockDetach (gameId, data, seqMismatch);
}

} // namespace xaya
