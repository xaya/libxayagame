// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pending.hpp"

#include "logic.hpp"

#include <glog/logging.h>

namespace nf
{

/* ************************************************************************** */

bool
PendingState::IsNewAsset (const Asset& a) const
{
  return assets.count (a) > 0;
}

void
PendingState::AddAsset (const Asset& a, const std::string* data)
{
  std::unique_ptr<std::string> dataCopy;
  if (data != nullptr)
    dataCopy = std::make_unique<std::string> (*data);

  const auto ins = assets.emplace (a, std::move (dataCopy));
  CHECK (ins.second) << "Asset was already in the pending map: " << a;
}

bool
PendingState::GetBalance (const Asset& a, const std::string& name,
                          Amount& balance) const
{
  const auto mit = balances.find ({name, a});
  if (mit == balances.end ())
    return false;

  balance = mit->second;
  return true;
}

void
PendingState::SetBalance (const Asset& a, const std::string& name,
                          const Amount balance)
{
  balances[{name, a}] = balance;
}

Json::Value
PendingState::ToJson () const
{
  Json::Value assetsJson(Json::arrayValue);
  for (const auto& entry : assets)
    {
      Json::Value cur(Json::objectValue);
      cur["asset"] = entry.first.ToJson ();
      if (entry.second == nullptr)
        cur["data"] = Json::Value ();
      else
        cur["data"] = *entry.second;
      assetsJson.append (cur);
    }

  Json::Value balancesJson(Json::objectValue);
  for (const auto& entry : balances)
    {
      Json::Value cur(Json::objectValue);
      cur["asset"] = entry.first.second.ToJson ();
      cur["balance"] = AmountToJson (entry.second);

      if (!balancesJson.isMember (entry.first.first))
        balancesJson[entry.first.first] = Json::Value (Json::arrayValue);
      balancesJson[entry.first.first].append (cur);
    }

  Json::Value res(Json::objectValue);
  res["assets"] = assetsJson;
  res["balances"] = balancesJson;

  return res;
}

/* ************************************************************************** */

void
PendingStateUpdater::UpdateBalance (const Asset& a, const std::string& name,
                                    const Amount num)
{
  state.SetBalance (a, name, GetBalance (a, name) + num);
}

void
PendingStateUpdater::ProcessMint (const Asset& a, const Amount supply,
                                  const std::string* data)
{
  state.AddAsset (a, data);
  if (supply > 0)
    UpdateBalance (a, a.GetMinter (), supply);
}

void
PendingStateUpdater::ProcessTransfer (const Asset& a, const Amount num,
                                      const std::string& sender,
                                      const std::string& recipient)
{
  UpdateBalance (a, sender, -num);
  UpdateBalance (a, recipient, num);
}

void
PendingStateUpdater::ProcessBurn (const Asset& a, const Amount num,
                                  const std::string& sender)
{
  UpdateBalance (a, sender, -num);
}

bool
PendingStateUpdater::AssetExists (const Asset& a) const
{
  if (state.IsNewAsset (a))
    return true;

  return MoveParser::AssetExists (a);
}

Amount
PendingStateUpdater::GetBalance (const Asset& a, const std::string& name) const
{
  Amount res;
  if (state.GetBalance (a, name, res))
    return res;

  return MoveParser::GetBalance (a, name);
}

/* ************************************************************************** */

PendingMoves::PendingMoves (NonFungibleLogic& rules)
  : xaya::SQLiteGame::PendingMoves(rules)
{}

void
PendingMoves::Clear ()
{
  state = PendingState ();
}

void
PendingMoves::AddPendingMove (const Json::Value& mv)
{
  const auto& db = AccessConfirmedState ();

  PendingStateUpdater updater(db, state);
  updater.ProcessOne (mv);
}

Json::Value
PendingMoves::ToJson () const
{
  return state.ToJson ();
}

/* ************************************************************************** */

} // namespace nf
