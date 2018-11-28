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
GameRpcServer::waitforchange ()
{
  LOG (INFO) << "RPC method called: waitforchange";

  uint256 block;
  game.WaitForChange (&block);

  /* If there is no best block so far, return JSON null.  */
  if (block.IsNull ())
    return Json::Value ();

  /* Otherwise, return the block hash.  */
  return block.ToHex ();
}

} // namespace xaya
