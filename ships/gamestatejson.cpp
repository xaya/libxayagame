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

  /* Export per-tier payment queues for the frontend.
     The SkillWager v3 contract needs this to construct queue snapshots.  */
  Json::Value queues(Json::objectValue);

  /* Get distinct tiers present in the queue.  */
  auto stmtTiers = db.PrepareRo (R"(
    SELECT DISTINCT `tier` FROM `payment_queue` ORDER BY `tier` ASC
  )");
  while (stmtTiers.Step ())
    {
      const int64_t tier = stmtTiers.Get<int64_t> (0);
      const std::string tierKey = std::to_string (tier);

      Json::Value entries(Json::arrayValue);
      auto stmtQ = db.PrepareRo (R"(
        SELECT `position`, `address`, `match_id`
          FROM `payment_queue`
          WHERE `tier` = ?1
          ORDER BY `position` ASC
          LIMIT 20
      )");
      stmtQ.Bind (1, tier);
      while (stmtQ.Step ())
        {
          Json::Value entry(Json::objectValue);
          entry["position"] = static_cast<Json::Int> (stmtQ.Get<int> (0));
          entry["address"] = stmtQ.Get<std::string> (1);
          entry["matchId"] = stmtQ.Get<std::string> (2);
          entries.append (entry);
        }

      auto stmtLen = db.PrepareRo (R"(
        SELECT COUNT(*) FROM `payment_queue` WHERE `tier` = ?1
      )");
      stmtLen.Bind (1, tier);
      stmtLen.Step ();

      Json::Value tierObj(Json::objectValue);
      tierObj["entries"] = entries;
      tierObj["length"] = static_cast<Json::Int> (stmtLen.Get<int> (0));
      queues[tierKey] = tierObj;
    }
  res["paymentqueues"] = queues;

  /* Also keep flat paymentqueue for backward compatibility.  */
  Json::Value queue(Json::arrayValue);
  auto stmtQ = db.PrepareRo (R"(
    SELECT `position`, `address`, `match_id`, `tier`
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
      entry["tier"] = static_cast<Json::Int64> (stmtQ.Get<int64_t> (3));
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
