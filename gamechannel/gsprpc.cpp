// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gsprpc.hpp"

#include "database.hpp"
#include "gamestatejson.hpp"

#include <xayagame/gamerpcserver.hpp>

#include <glog/logging.h>

#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>

#include <sqlite3.h>

namespace xaya
{

void
ChannelGspRpcServer::stop ()
{
  LOG (INFO) << "RPC method called: stop";
  game.RequestStop ();
}

Json::Value
ChannelGspRpcServer::getcurrentstate ()
{
  LOG (INFO) << "RPC method called: getcurrentstate";
  return game.GetCurrentJsonState ();
}

Json::Value
ChannelGspRpcServer::getpendingstate ()
{
  LOG (INFO) << "RPC method called: getpendingstate";
  return game.GetPendingJsonState ();
}

Json::Value
ChannelGspRpcServer::getchannel (const std::string& channelId)
{
  LOG (INFO) << "RPC method called: getchannel " << channelId;
  return DefaultGetChannel (game, chGame, channelId);
}

std::string
ChannelGspRpcServer::waitforchange (const std::string& knownBlock)
{
  LOG (INFO) << "RPC method called: waitforchange " << knownBlock;
  return GameRpcServer::DefaultWaitForChange (game, knownBlock);
}

Json::Value
ChannelGspRpcServer::waitforpendingchange (const int oldVersion)
{
  LOG (INFO) << "RPC method called: waitforpendingchange " << oldVersion;
  return game.WaitForPendingChange (oldVersion);
}

Json::Value
ChannelGspRpcServer::DefaultGetChannel (const Game& g, ChannelGame& chg,
                                        const std::string& channelId)
{
  uint256 id;
  if (!id.FromHex (channelId))
    throw jsonrpc::JsonRpcException (jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS,
                                     "channel ID is not a valid uint256");

  return chg.GetCustomStateData (g, "channel",
    [&chg, &id] (sqlite3* db)
      {
        ChannelsTable tbl(chg);
        auto h = tbl.GetById (id);

        if (h == nullptr)
          {
            LOG (WARNING) << "Channel is not known: " << id.ToHex ();
            return Json::Value ();
          }

        return ChannelToGameStateJson (*h, chg.GetBoardRules ());
      });
}

std::unique_ptr<RpcServerInterface>
ChannelGspInstanceFactory::BuildRpcServer (
    Game& game, jsonrpc::AbstractServerConnector& conn)
{
  std::unique_ptr<RpcServerInterface> res;
  res.reset (new WrappedRpcServer<ChannelGspRpcServer> (game, chGame, conn));
  return res;
}

} // namespace xaya
