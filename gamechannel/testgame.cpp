// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <gmock/gmock.h>

#include <glog/logging.h>

#include <sstream>

namespace xaya
{

namespace
{

using testing::_;
using testing::Return;

/**
 * The game state parsed into two integers.
 */
struct ParsedState
{
  int number;
  int count;
};

bool
ParsePair (const std::string& s, ParsedState& res)
{
  std::istringstream in(s);
  in >> res.number >> res.count;

  if (!in)
    {
      LOG (WARNING) << "Invalid game state: " << s;
      return false;
    }

  return true;
}

} // anonymous namespace

bool
AdditionRules::CompareStates (const proto::ChannelMetadata& meta,
                              const BoardState& a, const BoardState& b) const
{
  ParsedState pa, pb;
  if (!ParsePair (a, pa) || !ParsePair (b, pb))
    return false;

  return pa.number == pb.number && pa.count == pb.count;
}

int
AdditionRules::WhoseTurn (const proto::ChannelMetadata& meta,
                          const BoardState& state) const
{
  ParsedState p;
  CHECK (ParsePair (state, p)) << "WhoseTurn for invalid game state";

  if (p.number >= 100)
    return BoardRules::NO_TURN;

  return p.number % 2;
}

unsigned
AdditionRules::TurnCount (const proto::ChannelMetadata& meta,
                          const BoardState& state) const
{
  ParsedState p;
  CHECK (ParsePair (state, p)) << "TurnCount for invalid game state";

  return p.count;
}

bool
AdditionRules::ApplyMove (const proto::ChannelMetadata& meta,
                          const BoardState& oldState, const BoardMove& mv,
                          BoardState& newState) const
{
  ParsedState p;
  if (!ParsePair (oldState, p))
    return false;

  std::istringstream mvIn(mv);
  int add;
  mvIn >> add;
  if (add <= 0)
    return false;

  std::ostringstream out;
  out << (p.number + add) << " " << (p.count + 1);
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
  game.InitialiseGameContext (Chain::MAIN, "add", &rpcClient);
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
