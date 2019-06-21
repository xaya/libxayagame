// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include "schema.hpp"

#include <glog/logging.h>

namespace ships
{

const xaya::BoardRules&
ShipsLogic::GetBoardRules () const
{
  return boardRules;
}

void
ShipsLogic::SetupSchema (sqlite3* db)
{
  SetupGameChannelsSchema (db);
  SetupShipsSchema (db);
}

void
ShipsLogic::GetInitialStateBlock (unsigned& height, std::string& hashHex) const
{
  const xaya::Chain chain = GetChain ();
  switch (chain)
    {
    case xaya::Chain::MAIN:
      height = 930000;
      hashHex
          = "0b615b33ad3b4da22af2fe53553fbcc49ae00e27ae04c1d7e275c2a76d8f87d9";
      break;

    case xaya::Chain::TEST:
      height = 40000;
      hashHex
          = "74240aba644be39551e74c52eb4ffe6b63d1453c7d4cd1f6cfdee30d55354394";
      break;

    case xaya::Chain::REGTEST:
      height = 0;
      hashHex
          = "6f750b36d22f1dc3d0a6e483af45301022646dfc3b3ba2187865f5a7d6d83ab1";
      break;

    default:
      LOG (FATAL) << "Invalid chain value: " << static_cast<int> (chain);
    }
}

void
ShipsLogic::InitialiseState (sqlite3* db)
{
  /* The game simply starts with an empty database.  No stats for any names
     yet, and also no channels defined.  */
}

void
ShipsLogic::UpdateState (sqlite3* db, const Json::Value& blockData)
{
  LOG (WARNING) << "Updating the state for blocks is not yet implemented";
}

Json::Value
ShipsLogic::GetStateAsJson (sqlite3* db)
{
  LOG (WARNING) << "Returning empty JSON state for now";
  return Json::Value ();
}

} // namespace ships
