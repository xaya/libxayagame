// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "movesender.hpp"

#include <glog/logging.h>

namespace xaya
{

MoveSender::MoveSender (const std::string& gId,
                        const uint256& chId, const std::string& nm,
                        TransactionSender& s, OpenChannel& oc)
  : sender(s), game(oc), channelId(chId), playerName(nm), gameId(gId)
{
  jsonWriterBuilder["commentStyle"] = "None";
  jsonWriterBuilder["indentation"] = "";
  jsonWriterBuilder["enableYAMLCompatibility"] = false;
}

uint256
MoveSender::SendMove (const Json::Value& mv)
{
  Json::Value fullValue(Json::objectValue);
  fullValue["g"][gameId] = mv;

  const std::string strValue = Json::writeString (jsonWriterBuilder, fullValue);
  LOG (INFO) << "Sending move: " << playerName << "\n" << strValue;

  uint256 res;
  try
    {
      res = sender.SendRawMove (playerName, strValue);
    }
  catch (const std::exception& exc)
    {
      LOG (ERROR) << "SendMoveToBlockchain failed: " << exc.what ();
      res.SetNull ();
    }

  return res;
}

uint256
MoveSender::SendDispute (const proto::StateProof& proof)
{
  return SendMove (game.DisputeMove (channelId, proof));
}

uint256
MoveSender::SendResolution (const proto::StateProof& proof)
{
  return SendMove (game.ResolutionMove (channelId, proof));
}

} // namespace xaya
