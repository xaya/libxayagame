// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelgame.hpp"

#include "schema.hpp"

namespace xaya
{

void
ChannelGame::SetupGameChannelsSchema (sqlite3* db)
{
  InternalSetupGameChannelsSchema (db);
}

} // namespace xaya
