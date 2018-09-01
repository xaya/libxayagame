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

} // namespace xaya
