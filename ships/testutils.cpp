// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include "coord.hpp"

#include <glog/logging.h>

namespace ships
{

Grid
GridFromString (const std::string& str)
{
  CHECK_EQ (str.size (), Coord::CELLS);

  Grid g;
  for (int i = 0; i < Coord::CELLS; ++i)
    switch (str[i])
      {
      case '.':
        break;
      case 'x':
        g.Set (Coord (i));
        break;
      default:
        LOG (FATAL) << "Invalid character in position string: " << str[i];
      }

  return g;
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
