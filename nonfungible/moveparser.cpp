// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "moveparser.hpp"

#include "dbutils.hpp"

#include <glog/logging.h>

namespace nf
{

Amount
GetDbBalance (const xaya::SQLiteDatabase& db, const Asset& a,
              const std::string& name)
{
  auto stmt = db.PrepareRo (R"(
    SELECT `balance`
      FROM `balances`
      WHERE `name` = ?1 AND `minter` = ?2 AND `asset` = ?3
  )");
  BindParam (*stmt, 1, name);
  a.BindToParams (*stmt, 2, 3);

  if (!stmt.Step ())
    return 0;

  const Amount res = ColumnExtract<int64_t> (*stmt, 0);
  CHECK (!stmt.Step ());

  return res;
}

bool
MoveParser::AssetExists (const Asset& a) const
{
  auto stmt = db.PrepareRo (R"(
    SELECT COUNT(*)
      FROM `assets`
      WHERE `minter` = ?1 AND `asset` = ?2
  )");
  a.BindToParams (*stmt, 1, 2);

  CHECK (stmt.Step ());
  const auto count = ColumnExtract<int64_t> (*stmt, 0);
  CHECK (!stmt.Step ());

  CHECK_GE (count, 0);
  CHECK_LE (count, 1);

  return count > 0;
}

Amount
MoveParser::GetBalance (const Asset& a, const std::string& name) const
{
  return GetDbBalance (db, a, name);
}

void
MoveParser::HandleOperation (const std::string& name, const Json::Value& mv)
{
  CHECK (mv.isObject ());
  if (mv.size () != 1)
    {
      LOG (WARNING) << "Invalid operation: " << mv;
      return;
    }

  if (mv.isMember ("m"))
    HandleMint (name, mv["m"]);
  else if (mv.isMember ("t"))
    HandleTransfer (name, mv["t"]);
  else if (mv.isMember ("b"))
    HandleBurn (name, mv["b"]);
  else
    LOG (WARNING) << "Invalid operation: " << mv;
}

void
MoveParser::HandleMint (const std::string& name, const Json::Value& op)
{
  if (!op.isObject ())
    {
      LOG (WARNING) << "Invalid mint operation: " << op;
      return;
    }

  const bool hasData = op.isMember ("d");
  if ((hasData && op.size () != 3) || (!hasData && op.size () != 2))
    {
      LOG (WARNING) << "Invalid mint operation: " << op;
      return;
    }

  const auto& assetNameVal = op["a"];
  if (!assetNameVal.isString ())
    {
      LOG (WARNING) << "Invalid asset in mint: " << op;
      return;
    }
  const std::string assetName = assetNameVal.asString ();
  if (!Asset::IsValidName (assetName))
    {
      LOG (WARNING) << "Invalid asset in mint: " << op;
      return;
    }
  const Asset a(name, assetName);

  Amount supply;
  if (!AmountFromJson (op["n"], supply))
    {
      LOG (WARNING) << "Invalid supply in mint: " << op;
      return;
    }

  std::string data;
  if (hasData)
    {
      const auto& dataVal = op["d"];
      if (!dataVal.isString ())
        {
          LOG (WARNING) << "Invalid data in mint: " << op;
          return;
        }
      data = dataVal.asString ();
    }

  if (AssetExists (a))
    {
      LOG (WARNING) << "Mint of already existing asset " << a << ": " << op;
      return;
    }

  ProcessMint (a, supply, hasData ? &data : nullptr);
}

void
MoveParser::HandleTransfer (const std::string& name, const Json::Value& op)
{
  if (!op.isObject () || op.size () != 3)
    {
      LOG (WARNING) << "Invalid transfer operation: " << op;
      return;
    }

  Asset a;
  if (!a.FromJson (op["a"]))
    {
      LOG (WARNING) << "Invalid asset in transfer: " << op;
      return;
    }

  Amount n;
  if (!AmountFromJson (op["n"], n) || n <= 0)
    {
      LOG (WARNING) << "Invalid amount in transfer: " << op;
      return;
    }

  const auto& recvVal = op["r"];
  if (!recvVal.isString ())
    {
      LOG (WARNING) << "Invalid recipient in transfer: " << op;
      return;
    }

  const Amount balance = GetBalance (a, name);
  if (n > balance)
    {
      LOG (WARNING)
          << "User " << name << " only owns " << balance << " of " << a
          << ", can't transfer " << n;
      return;
    }

  ProcessTransfer (a, n, name, recvVal.asString ());
}

void
MoveParser::HandleBurn (const std::string& name, const Json::Value& op)
{
  if (!op.isObject () || op.size () != 2)
    {
      LOG (WARNING) << "Invalid burn operation: " << op;
      return;
    }

  Asset a;
  if (!a.FromJson (op["a"]))
    {
      LOG (WARNING) << "Invalid asset in burn: " << op;
      return;
    }

  Amount n;
  if (!AmountFromJson (op["n"], n) || n <= 0)
    {
      LOG (WARNING) << "Invalid amount in burn: " << op;
      return;
    }

  const Amount balance = GetBalance (a, name);
  if (n > balance)
    {
      LOG (WARNING)
          << "User " << name << " only owns " << balance << " of " << a
          << ", can't burn " << n;
      return;
    }

  ProcessBurn (a, n, name);
}

void
MoveParser::ProcessOne (const Json::Value& obj)
{
  CHECK (obj.isObject ());

  const auto& nameVal = obj["name"];
  CHECK (nameVal.isString ());
  const std::string name = nameVal.asString ();

  const auto& mv = obj["move"];

  if (mv.isObject ())
    HandleOperation (name, mv);
  else if (mv.isArray ())
    {
      for (const auto& op : mv)
        {
          if (op.isObject ())
            HandleOperation (name, op);
          else
            LOG (WARNING) << "Invalid operation inside array move: " << op;
        }
    }
  else
    LOG (WARNING) << "Invalid move: " << mv;
}

} // namespace nf
