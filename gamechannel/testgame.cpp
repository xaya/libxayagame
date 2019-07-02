// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include "signatures.hpp"

#include <xayautil/base64.hpp>

#include <gmock/gmock.h>

#include <glog/logging.h>

#include <sstream>

namespace xaya
{

namespace
{

using testing::_;
using testing::Return;

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

class AdditionState : public ParsedBoardState
{

private:

  const ParsedState data;

public:

  explicit AdditionState (const ParsedState& d)
    : data(d)
  {}

  AdditionState () = delete;
  AdditionState (const AdditionState&) = delete;
  void operator= (const AdditionState&) = delete;

  bool
  Equals (const BoardState& other) const override
  {
    ParsedState p;
    if (!ParsePair (other, p))
      return false;

    return p.number == data.number && p.count == data.count;
  }

  int
  WhoseTurn () const override
  {
    if (data.number >= 100)
      return ParsedBoardState::NO_TURN;

    return data.number % 2;
  }

  unsigned
  TurnCount () const override
  {
    return data.count;
  }

  bool
  ApplyMove (XayaRpcClient& rpc, const BoardMove& mv,
             BoardState& newState) const override
  {
    /* The game-channel engine should never invoke ApplyMove on a 'no turn'
       situation.  Make sure to verify that.  */
    CHECK (WhoseTurn () != ParsedBoardState::NO_TURN);

    std::istringstream mvIn(mv);
    int add;
    mvIn >> add;
    if (add <= 0)
      return false;

    std::ostringstream out;
    out << (data.number + add) << " " << (data.count + 1);
    newState = out.str ();

    return true;
  }

  Json::Value
  ToJson () const override
  {
    Json::Value res(Json::objectValue);
    res["number"] = data.number;
    res["count"] = data.count;
    return res;
  }

};

} // anonymous namespace

std::unique_ptr<ParsedBoardState>
AdditionRules::ParseState (const uint256& channelId,
                           const proto::ChannelMetadata& meta,
                           const BoardState& state) const
{
  ParsedState p;
  if (!ParsePair (state, p))
    return nullptr;

  return std::make_unique<AdditionState> (p);
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
TestGameFixture::ExpectSignature (const uint256& channelId,
                                  const proto::ChannelMetadata& meta,
                                  const std::string& topic,
                                  const std::string& msg,
                                  const std::string& sgn,
                                  const std::string& addr)
{
  Json::Value res(Json::objectValue);
  res["valid"] = true;
  res["address"] = addr;

  const std::string hashed
      = GetChannelSignatureMessage (channelId, meta, topic, msg);
  EXPECT_CALL (mockXayaServer, verifymessage ("", hashed, EncodeBase64 (sgn)))
      .WillOnce (Return (res));
}

} // namespace xaya
