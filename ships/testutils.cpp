// Copyright (C) 2019 The Xaya developers
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
  : httpServer(xaya::MockXayaRpcServer::HTTP_PORT),
    httpClient(xaya::MockXayaRpcServer::HTTP_URL),
    mockXayaServer(httpServer),
    rpcClient(httpClient)
{
  game.Initialise (":memory:");
  game.InitialiseGameContext (xaya::Chain::MAIN, "xs", &rpcClient);
  game.GetStorage ()->Initialise ();
  /* The initialisation above already sets up the database schema.  */

  mockXayaServer.StartListening ();
}

InMemoryLogicFixture::~InMemoryLogicFixture ()
{
  mockXayaServer.StopListening ();
}

sqlite3*
InMemoryLogicFixture::GetDb ()
{
  return game.GetDatabaseForTesting ();
}

} // namespace ships
