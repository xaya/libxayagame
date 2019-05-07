// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include <glog/logging.h>

namespace xaya
{

void
TestGame::SetupSchema (sqlite3* db)
{
  SetupGameChannelsSchema (db);
}

void
TestGame::GetInitialStateBlock (unsigned& height, std::string& hashHex) const
{
  LOG (FATAL) << "TestGame::GetInitialStateBlock is not implemented";
}

void
TestGame::InitialiseState (sqlite3* db)
{
  LOG (FATAL) << "TestGame::InitialiseState is not implemented";
}

void
TestGame::UpdateState (sqlite3* db, const Json::Value& blockData)
{
  LOG (FATAL) << "TestGame::UpdateState is not implemented";
}

Json::Value
TestGame::GetStateAsJson (sqlite3* db)
{
  LOG (FATAL) << "TestGame::GetStateAsJson is not implemented";
}

TestGameFixture::TestGameFixture ()
{
  game.Initialise (":memory:");
  game.GetStorage ()->Initialise ();

  /* The initialisation above already sets up the database schema.  */
}

sqlite3*
TestGameFixture::GetDb ()
{
  return game.GetDatabaseForTesting ();
}

} // namespace xaya
