// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include <glog/logging.h>

#include <sstream>

namespace xaya
{

namespace
{

int
ParseNum (const std::string& s)
{
  std::istringstream in(s);
  int res;
  in >> res;

  return res;
}

} // anonymous namespace

bool
AdditionRules::CompareStates (const ChannelMetadata& meta,
                              const BoardState& a, const BoardState& b) const
{
  return ParseNum (a) == ParseNum (b);
}

int
AdditionRules::WhoseTurn (const ChannelMetadata& meta,
                          const BoardState& state) const
{
  const int num = ParseNum (state);
  if (num >= 100)
    return BoardRules::NO_TURN;

  return num % 2;
}

bool
AdditionRules::ApplyMove (const ChannelMetadata& meta,
                          const BoardState& oldState, const BoardMove& mv,
                          BoardState& newState) const
{
  const int num = ParseNum (oldState);
  /* The framework code should never actually attempt to apply a move in
     a NO_TURN situation.  Verify that.  */
  CHECK_LT (num, 100) << "Move applied to 'no turn' state";

  const int add = ParseNum (mv);
  if (add <= 0)
    return false;

  std::ostringstream out;
  out << (num + add);
  newState = out.str ();

  return true;
}

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

const BoardRules&
TestGame::GetBoardRules () const
{
  return rules;
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
