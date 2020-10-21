// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamestatejson.hpp"

#include <gamechannel/database.hpp>
#include <gamechannel/gamestatejson.hpp>

#include <sqlite3.h>

#include <glog/logging.h>

namespace ships
{

Json::Value
GameStateJson::GetFullJson () const
{
  Json::Value stats(Json::objectValue);
  auto stmt = db.PrepareRo (R"(
    SELECT `name`, `won`, `lost`
      FROM `game_stats`
  )");
  while (stmt.Step ())
    {
      const int len = sqlite3_column_bytes (*stmt, 0);
      const unsigned char* bytes = sqlite3_column_text (*stmt, 0);
      const std::string name(reinterpret_cast<const char*> (bytes), len);

      Json::Value cur(Json::objectValue);
      cur["won"] = sqlite3_column_int (*stmt, 1);
      cur["lost"] = sqlite3_column_int (*stmt, 2);

      stats[name] = cur;
    }

  Json::Value res(Json::objectValue);
  res["gamestats"] = stats;

  xaya::ChannelsTable tbl(const_cast<xaya::SQLiteDatabase&> (db));
  res["channels"] = xaya::AllChannelsGameStateJson (tbl, rules);

  return res;
}

} // namespace ships
