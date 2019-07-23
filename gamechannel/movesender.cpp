// Copyright (C) 2019 The Xaya developers
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
  /* Note that we could optimise this by requesting just name_pending for
     the MoveSender's player name.  For now, there are two reasons why we
     don't do that:  First, Xaya Core's (and Namecoin's) name_pending
     implementation goes through the full mempool anyway instead of using
     its index of mempool names; so while we would save some transfer of
     data and processing here, we would still access the whole mempool
     at some point.  And second, if we defined name_pending as accepting
     a string argument in the RPC stubs, we couldn't call the general form
     anymore; that might be useful in other places.

     But in case Namecoin gets updated to use the indices for name_pending
     and perhaps to allow an empty name to signal "no filtering" (instead of
     just a null value), we could change the implementation here without
     any impact on clients.  */
  const auto pending = rpc.name_pending ();

  for (const auto& p : pending)
    if (p["txid"].asString () == txid.ToHex ())
      {
        CHECK_EQ (p["name"], playerName);
        return true;
      }

  return false;
}

} // namespace xaya
