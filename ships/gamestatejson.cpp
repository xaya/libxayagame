// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamestatejson.hpp"

#include <gamechannel/database.hpp>
#include <gamechannel/gamestatejson.hpp>

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
      const auto name = stmt.Get<std::string> (0);

      Json::Value cur(Json::objectValue);
      cur["won"] = static_cast<Json::Int64> (stmt.Get<int64_t> (1));
      cur["lost"] = static_cast<Json::Int64> (stmt.Get<int64_t> (2));

      stats[name] = cur;
    }

  Json::Value res(Json::objectValue);
  res["gamestats"] = stats;

  xaya::ChannelsTable tbl(const_cast<xaya::SQLiteDatabase&> (db));
  res["channels"] = xaya::AllChannelsGameStateJson (tbl, rules);

  /* Export the payment queue (first 20 entries) for the frontend.
     The SkillWager contract needs this to construct queue snapshots.  */
  Json::Value queue(Json::arrayValue);
  auto stmtQ = db.PrepareRo (R"(
    SELECT `position`, `address`, `match_id`
      FROM `payment_queue`
      ORDER BY `position` ASC
      LIMIT 20
  )");
  while (stmtQ.Step ())
    {
      Json::Value entry(Json::objectValue);
      entry["position"] = static_cast<Json::Int> (stmtQ.Get<int> (0));
      entry["address"] = stmtQ.Get<std::string> (1);
      entry["matchId"] = stmtQ.Get<std::string> (2);
      queue.append (entry);
    }
  res["paymentqueue"] = queue;

  auto stmtLen = db.PrepareRo (R"(
    SELECT COUNT(*) FROM `payment_queue`
  )");
  stmtLen.Step ();
  res["paymentqueuelength"]
      = static_cast<Json::Int> (stmtLen.Get<int> (0));

  return res;
}

} // namespace ships
