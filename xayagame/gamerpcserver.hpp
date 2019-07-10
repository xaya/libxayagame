// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_GAMERPCSERVER_HPP
#define XAYAGAME_GAMERPCSERVER_HPP

#include "game.hpp"

#include "rpc-stubs/gamerpcserverstub.h"

#include <json/json.h>
#include <jsonrpccpp/server.h>

namespace xaya
{

/**
 * Implementation of the basic RPC interface that games can expose.  It just
 * supports the generic "stop" and "getcurrentstate" methods, by calling the
 * corresponding functions on a Game instance.
 *
 * This can be used by games that only need this basic, general interface.
 * Games which want to expose additional specific functions should create
 * their own implementation and may use the Game functions directly for
 * implementing them.
 */
class GameRpcServer : public GameRpcServerStub
{

private:

  /** The game instance whose methods we expose through RPC.  */
  Game& game;

public:

  explicit GameRpcServer (Game& g, jsonrpc::AbstractServerConnector& conn)
    : GameRpcServerStub(conn), game(g)
  {}

  virtual void stop () override;
  virtual Json::Value getcurrentstate () override;
  virtual std::string waitforchange (const std::string& knownBlock) override;

  /**
   * Implements the standard waitforchange RPC method independent of a
   * particular server instance.  This can be used by customised RPC servers
   * of games that have more methods, so that the code can still be reused.
   */
  static std::string DefaultWaitForChange (const Game& g,
                                           const std::string& knownBlock);

};

} // namespace xaya

#endif // XAYAGAME_GAMERPCSERVER_HPP
