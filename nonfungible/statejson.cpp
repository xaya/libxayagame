// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "statejson.hpp"

#include "dbutils.hpp"
#include "moveparser.hpp"

#include <glog/logging.h>

namespace nf
{

Json::Value
StateJsonExtractor::ListAssets () const
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `minter`, `asset`
      FROM `assets`
      ORDER BY `minter`, `asset`
  )");

  Json::Value res(Json::arrayValue);
  while (StepStatement (stmt))
    res.append (Asset::FromColumns (stmt, 0, 1).ToJson ());

  return res;
}

Json::Value
StateJsonExtractor::GetAssetDetails (const Asset& a) const
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `data`
      FROM `assets`
      WHERE `minter` = ?1 AND `asset` = ?2
  )");
  a.BindToParams (stmt, 1, 2);

  if (!StepStatement (stmt))
    return Json::Value ();

  Json::Value data;
  if (!ColumnIsNull (stmt, 0))
    data = ColumnExtract<std::string> (stmt, 0);

  CHECK (!StepStatement (stmt));

  stmt = db.PrepareRo (R"(
    SELECT `name`, `balance`
      FROM `balances`
      WHERE `minter` = ?1 AND `asset` = ?2
      ORDER BY `name`
  )");
  a.BindToParams (stmt, 1, 2);

  Json::Value balances(Json::objectValue);
  Amount total = 0;
  while (StepStatement (stmt))
    {
      const auto name = ColumnExtract<std::string> (stmt, 0);
      const Amount val = ColumnExtract<int64_t> (stmt, 1);

      CHECK_GT (val, 0);
      CHECK (!balances.isMember (name)) << "Duplicate user: " << name;

      total += val;
      balances[name] = val;
    }

  Json::Value res(Json::objectValue);
  res["asset"] = a.ToJson ();
  res["data"] = data;
  res["supply"] = AmountToJson (total);
  res["balances"] = balances;

  return res;
}

Json::Value
StateJsonExtractor::GetBalance (const Asset& a, const std::string& name) const
{
  return AmountToJson (GetDbBalance (db, a, name));
}

Json::Value
StateJsonExtractor::GetUserBalances (const std::string& name) const
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `minter`, `asset`, `balance`
      FROM `balances`
      WHERE `name` = ?1
      ORDER BY `minter`, `asset`
  )");
  BindParam (stmt, 1, name);

  Json::Value res(Json::arrayValue);
  while (StepStatement (stmt))
    {
      const auto asset = Asset::FromColumns (stmt, 0, 1);
      const Amount balance = ColumnExtract<int64_t> (stmt, 2);

      Json::Value cur(Json::objectValue);
      cur["asset"] = asset.ToJson ();
      cur["balance"] = AmountToJson (balance);

      res.append (cur);
    }

  return res;
}

Json::Value
StateJsonExtractor::FullState () const
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `minter`, `asset`
      FROM `assets`
      ORDER BY `minter`, `asset`
  )");

  Json::Value res(Json::arrayValue);
  while (StepStatement (stmt))
    {
      const auto asset = Asset::FromColumns (stmt, 0, 1);
      res.append (GetAssetDetails (asset));
    }

  return res;
}

} // namespace nf
