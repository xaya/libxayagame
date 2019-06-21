// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include "schema.hpp"

#include <gamechannel/database.hpp>

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
  const auto& moves = blockData["moves"];
  CHECK (moves.isArray ());
  LOG (INFO) << "Processing " << moves.size () << " moves...";
  for (const auto& mv : moves)
    {
      CHECK (mv.isObject ()) << "Not an object: " << mv;

      const auto& nameVal = mv["name"];
      CHECK (nameVal.isString ());
      const std::string name = nameVal.asString ();

      const auto& txidVal = mv["txid"];
      CHECK (txidVal.isString ());
      xaya::uint256 txid;
      CHECK (txid.FromHex (txidVal.asString ()));

      const auto& data = mv["move"];
      if (!data.isObject ())
        {
          LOG (WARNING) << "Move by " << name << " is not an object: " << data;
          continue;
        }

      /* Some of the possible moves can interact with each other (e.g. joining
         a channel and filing a dispute immediately).  These interactions
         are not generally useful, and just complicate things (as we have to
         ensure that the order remains fixed and they keep working).  Thus
         let us simply forbid more than one action per move.  */
      if (data.size () > 1)
        {
          LOG (WARNING)
              << "Move by " << name << " has more than one action: " << data;
          continue;
        }

      HandleCreateChannel (data["c"], name, txid);
    }

  /* TODO: Go through expired disputes and declare winners for them.  */
}

void
ShipsLogic::HandleCreateChannel (const Json::Value& obj,
                                 const std::string& name,
                                 const xaya::uint256& txid)
{
  if (!obj.isObject ())
    return;

  const auto& addrVal = obj["addr"];
  if (obj.size () != 1 || !addrVal.isString ())
    {
      LOG (WARNING) << "Invalid create channel move: " << obj;
      return;
    }
  const std::string addr = addrVal.asString ();

  LOG (INFO)
      << "Creating channel with ID " << txid.ToHex ()
      << " for user " << name << " with address " << addr;

  xaya::ChannelsTable tbl(*this);

  /* Verify that this is indeed a new instance and not an existing one.  That
     should never happen, assuming that txid's do not collide.  */
  auto h = tbl.GetById (txid);
  CHECK (h == nullptr) << "Already have channel with ID " << txid.ToHex ();

  h = tbl.CreateNew (txid);
  auto* p = h->MutableMetadata ().add_participants ();
  p->set_name (name);
  p->set_address (addr);
}

Json::Value
ShipsLogic::GetStateAsJson (sqlite3* db)
{
  LOG (WARNING) << "Returning empty JSON state for now";
  return Json::Value ();
}

} // namespace ships
