// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "movesender.hpp"

#include <jsonrpccpp/common/exception.h>

#include <glog/logging.h>

namespace xaya
{

MoveSender::MoveSender (const std::string& gId,
                        const uint256& chId, const std::string& nm,
                        XayaRpcClient& r, XayaWalletRpcClient& w,
                        OpenChannel& oc)
  : rpc(r), wallet(w), game(oc), channelId(chId),
    playerName("p/" + nm), gameId(gId)
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
  uint256 res;
  try
    {
      LOG (INFO)
          << "Sending move: name_update " << playerName
          << "\n" << strValue;

      const std::string txidHex = wallet.name_update (playerName, strValue);
      LOG (INFO) << "Success, name txid = " << txidHex;

      CHECK (res.FromHex (txidHex));
    }
  catch (const jsonrpc::JsonRpcException& exc)
    {
      LOG (ERROR) << "name_update failed: " << exc.what ();
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

bool
MoveSender::IsPending (const uint256& txid) const
{
  const std::string txidHex = txid.ToHex ();

  const auto mempool = rpc.getrawmempool ();
  for (const auto& tx : mempool)
    if (tx.asString () == txidHex)
      return true;

  return false;
}

} // namespace xaya
