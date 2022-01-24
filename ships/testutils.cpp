// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include "coord.hpp"

#include <glog/logging.h>

#include <sstream>

namespace ships
{

Json::Value
ParseJson (const std::string& str)
{
  std::istringstream in(str);

  Json::Value res;
  in >> res;
  CHECK (in);

  return res;
}

InMemoryLogicFixture::InMemoryLogicFixture ()
  : game(verifier)
{
  game.Initialise (":memory:");
  game.InitialiseGameContext (xaya::Chain::MAIN, "xs", nullptr);
  game.GetStorage ().Initialise ();
  /* The initialisation above already sets up the database schema.  */
}

xaya::SQLiteDatabase&
InMemoryLogicFixture::GetDb ()
{
  return game.GetDatabaseForTesting ();
}

const ShipsBoardRules&
InMemoryLogicFixture::GetBoardRules () const
{
  return dynamic_cast<const ShipsBoardRules&> (game.GetBoardRules ());
}

} // namespace ships
