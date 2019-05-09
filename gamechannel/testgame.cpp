// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <gmock/gmock.h>

#include <glog/logging.h>

#include <sstream>
#include <utility>

namespace xaya
{

namespace
{

using testing::_;
using testing::Return;

std::pair<int, int>
ParsePair (const std::string& s)
{
  std::istringstream in(s);
  int a, b;
  in >> a >> b;

  /* Make sure that we actually had two numbers to read.  Else we would get
     one or both as "silent zeros", but that means there's a bug somewhere
     in the test data.  */
  CHECK (in) << "Invalid game state: " << s;

  return {a, b};
}

} // anonymous namespace

bool
AdditionRules::CompareStates (const ChannelMetadata& meta,
                              const BoardState& a, const BoardState& b) const
{
  return ParsePair (a) == ParsePair (b);
}

int
AdditionRules::WhoseTurn (const ChannelMetadata& meta,
                          const BoardState& state) const
{
  const int num = ParsePair (state).first;
  if (num >= 100)
    return BoardRules::NO_TURN;

  return num % 2;
}

unsigned
AdditionRules::TurnCount (const ChannelMetadata& meta,
                          const BoardState& state) const
{
  return ParsePair (state).second;
}

bool
AdditionRules::ApplyMove (const ChannelMetadata& meta,
                          const BoardState& oldState, const BoardMove& mv,
                          BoardState& newState) const
{
  const auto state = ParsePair (oldState);
  /* The framework code should never actually attempt to apply a move in
     a NO_TURN situation.  Verify that.  */
  CHECK_LT (state.first, 100) << "Move applied to 'no turn' state";

  std::istringstream mvIn(mv);
  int add;
  mvIn >> add;
  if (add <= 0)
    return false;

  std::ostringstream out;
  out << (state.first + add) << " " << (state.second + 1);
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
  : httpServer(MockXayaRpcServer::HTTP_PORT),
    httpClient(MockXayaRpcServer::HTTP_URL),
    mockXayaServer(httpServer),
    rpcClient(httpClient)
{
  game.Initialise (":memory:");
  game.GetStorage ()->Initialise ();
  /* The initialisation above already sets up the database schema.  */

  mockXayaServer.StartListening ();
}

TestGameFixture::~TestGameFixture ()
{
  mockXayaServer.StopListening ();
}

sqlite3*
TestGameFixture::GetDb ()
{
  return game.GetDatabaseForTesting ();
}

void
TestGameFixture::ValidSignature (const std::string& sgn,
                                 const std::string& addr)
{
  Json::Value res(Json::objectValue);
  res["valid"] = true;
  res["address"] = addr;

  EXPECT_CALL (mockXayaServer, verifymessage ("", _, EncodeBase64 (sgn)))
      .WillRepeatedly (Return (res));
}

void
TestGameFixture::ExpectSignature (const std::string& msg,
                                  const std::string& sgn,
                                  const std::string& addr)
{
  Json::Value res(Json::objectValue);
  res["valid"] = true;
  res["address"] = addr;

  const std::string hashed = SHA256::Hash (msg).ToHex ();
  EXPECT_CALL (mockXayaServer, verifymessage ("", hashed, EncodeBase64 (sgn)))
      .WillOnce (Return (res));
}

} // namespace xaya
