// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamerpcserver.hpp"

#include <glog/logging.h>

namespace xaya
{

void
GameRpcServer::stop ()
{
  LOG (INFO) << "RPC method called: stop";
  game.RequestStop ();
}

Json::Value
GameRpcServer::getcurrentstate ()
{
  LOG (INFO) << "RPC method called: getcurrentstate";
  return game.GetCurrentJsonState ();
}

Json::Value
GameRpcServer::waitforchange (const std::string& knownBlock)
{
  LOG (INFO) << "RPC method called: waitforchange " << knownBlock;
  return DefaultWaitForChange (game, knownBlock);
}

Json::Value
GameRpcServer::DefaultWaitForChange (const Game& g,
                                     const std::string& knownBlock)
{
  LOG (INFO) << "RPC method called: waitforchange " << knownBlock;

  uint256 oldBlock;
  oldBlock.SetNull ();
  if (!knownBlock.empty () && !oldBlock.FromHex (knownBlock))
    LOG (ERROR)
        << "Invalid block hash passed as known block: " << knownBlock;

  uint256 newBlock;
  g.WaitForChange (oldBlock, newBlock);

  /* If there is no best block so far, return JSON null.  */
  if (newBlock.IsNull ())
    return Json::Value ();

  /* Otherwise, return the block hash.  */
  return newBlock.ToHex ();
}

} // namespace xaya
