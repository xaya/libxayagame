// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamestatejson.hpp"

#include <gamechannel/database.hpp>
#include <gamechannel/gamestatejson.hpp>

#include <sqlite3.h>

namespace ships
{

Json::Value
GameStateJson::GetFullJson ()
{
  Json::Value stats(Json::objectValue);
  auto* stmt = rules.PrepareStatement (R"(
    SELECT `name`, `won`, `lost`
      FROM `game_stats`
  )");
  while (true)
    {
      const int rc = sqlite3_step (stmt);
      if (rc == SQLITE_DONE)
        break;
      CHECK_EQ (rc, SQLITE_ROW);

      const int len = sqlite3_column_bytes (stmt, 0);
      const unsigned char* bytes = sqlite3_column_text (stmt, 0);
      const std::string name(reinterpret_cast<const char*> (bytes), len);

      Json::Value cur(Json::objectValue);
      cur["won"] = sqlite3_column_int (stmt, 1);
      cur["lost"] = sqlite3_column_int (stmt, 2);

      stats[name] = cur;
    }

  Json::Value res(Json::objectValue);
  res["gamestats"] = stats;

  xaya::ChannelsTable tbl(rules);
  res["channels"] = xaya::AllChannelsGameStateJson (tbl, rules.boardRules);

  return res;
}

} // namespace ships
