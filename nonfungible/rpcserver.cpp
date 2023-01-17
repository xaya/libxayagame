// Copyright (C) 2020-2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.hpp"

#include "statejson.hpp"

#include "xayagame/gamerpcserver.hpp"
#include "xayagame/sqliteintro.hpp"
#include "xayautil/hash.hpp"

#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>

#include <glog/logging.h>

namespace nf
{

namespace
{

/**
 * Tries to parse a JSON value as Asset, and throws a JSON-RPC exception
 * if it doesn't work.
 */
Asset
GetAsset (const Json::Value& val)
{
  Asset res;
  if (!res.FromJson (val))
    {
      std::ostringstream out;
      out << "invalid asset spec: " << val;
      throw jsonrpc::JsonRpcException (
          jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS, out.str ());
    }

  return res;
}

} // anonymous namespace

void
RpcServer::stop ()
{
  LOG (INFO) << "RPC method called: stop";
  game.RequestStop ();
}

Json::Value
RpcServer::getcurrentstate ()
{
  LOG (INFO) << "RPC method called: getcurrentstate";
  return game.GetCurrentJsonState ();
}

Json::Value
RpcServer::getnullstate ()
{
  LOG (INFO) << "RPC method called: getnullstate";
  return game.GetNullJsonState ();
}

Json::Value
RpcServer::getpendingstate ()
{
  LOG (INFO) << "RPC method called: getpendingstate";
  return game.GetPendingJsonState ();
}

Json::Value
RpcServer::hashcurrentstate ()
{
  LOG (INFO) << "RPC method called: hashcurrentstate";
  return logic.GetCustomStateData (game, "data",
      [] (const xaya::SQLiteDatabase& db)
        {
          xaya::SHA256 h;
          WriteAllTables (h, db);
          return h.Finalise ().ToHex ();
        });
}

Json::Value
RpcServer::getstatehash (const std::string& block)
{
  LOG (INFO) << "RPC method called: getstatehash " << block;
  if (hasher == nullptr)
    throw jsonrpc::JsonRpcException (
        jsonrpc::Errors::ERROR_RPC_METHOD_NOT_FOUND,
        "state hashing is not enabled");

  xaya::uint256 blockHash;
  if (!blockHash.FromHex (block))
    throw jsonrpc::JsonRpcException (
        jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS, "invalid block hash");

  return logic.GetCustomStateData (game, "data",
      [this, &blockHash] (const xaya::SQLiteDatabase& db) -> Json::Value
        {
          xaya::uint256 value;
          if (!hasher->GetHash (db, blockHash, value))
            return Json::Value ();
          return value.ToHex ();
        });
}

void
RpcServer::settargetblock (const std::string& block)
{
  LOG (INFO) << "RPC method called: settargetblock " << block;

  xaya::uint256 hash;
  if (block.empty ())
    hash.SetNull ();
  else if (!hash.FromHex (block))
    {
      LOG (WARNING) << "Invalid block hash for settargetblock";
      return;
    }

  game.SetTargetBlock (hash);
}

std::string
RpcServer::waitforchange (const std::string& knownBlock)
{
  LOG (INFO) << "RPC method called: waitforchange " << knownBlock;
  return xaya::GameRpcServer::DefaultWaitForChange (game, knownBlock);
}

Json::Value
RpcServer::waitforpendingchange (const int knownVersion)
{
  LOG (INFO) << "RPC method called: waitforpendingchange " << knownVersion;
  return game.WaitForPendingChange (knownVersion);
}

Json::Value
RpcServer::listassets ()
{
  LOG (INFO) << "RPC method called: listassets";
  return logic.GetCustomStateData (game,
      [] (const StateJsonExtractor& ext)
        {
          return ext.ListAssets ();
        });
}

Json::Value
RpcServer::getassetdetails (const Json::Value& assetVal)
{
  const auto asset = GetAsset (assetVal);
  LOG (INFO) << "RPC method called: getassetdetails " << asset;
  return logic.GetCustomStateData (game,
      [&asset] (const StateJsonExtractor& ext)
        {
          return ext.GetAssetDetails (asset);
        });
}

Json::Value
RpcServer::getbalance (const Json::Value& assetVal, const std::string& name)
{
  const auto asset = GetAsset (assetVal);
  LOG (INFO) << "RPC method called: getbalance " << asset << " " << name;
  return logic.GetCustomStateData (game,
      [&asset, &name] (const StateJsonExtractor& ext)
        {
          return ext.GetBalance (asset, name);
        });
}

Json::Value
RpcServer::getuserbalances (const std::string& name)
{
  LOG (INFO) << "RPC method called: getuserbalances " << name;
  return logic.GetCustomStateData (game,
      [&name] (const StateJsonExtractor& ext)
        {
          return ext.GetUserBalances (name);
        });
}

} // namespace nf
