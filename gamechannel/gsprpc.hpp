// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_GSPRPC_HPP
#define GAMECHANNEL_GSPRPC_HPP

#include "channelgame.hpp"

#include "rpc-stubs/channelgsprpcserverstub.h"

#include <xayagame/defaultmain.hpp>
#include <xayagame/game.hpp>

#include <json/json.h>
#include <jsonrpccpp/server.h>

namespace xaya
{

/**
 * Implementation of a simple RPC server for game channel GSPs.  This extends
 * the GameRpcServer for general GSPs by the "getchannel" method, which extracts
 * data about a single channel by ID.  This method is used by the channel
 * daemon to query states.
 */
class ChannelGspRpcServer : public ChannelGspRpcServerStub
{

private:

  /** The game instance whose methods we expose through RPC.  */
  Game& game;

  /** The ChannelGame that manages the database.  */
  ChannelGame& chGame;

public:

  explicit ChannelGspRpcServer (Game& g, ChannelGame& chg,
                                jsonrpc::AbstractServerConnector& conn)
    : ChannelGspRpcServerStub(conn), game(g), chGame(chg)
  {}

  virtual void stop () override;
  virtual Json::Value getcurrentstate () override;
  virtual Json::Value getchannel (const std::string& channelId) override;
  virtual std::string waitforchange (const std::string& knownBlock) override;

  /**
   * Implements the standard getchannel method.  This can be used by
   * games that have an extended RPC server for their GSPs but want to
   * provide the standard getchannel.
   */
  static Json::Value DefaultGetChannel (const Game& g, ChannelGame& chg,
                                        const std::string& channelId);

};

/**
 * Customised instance factory for a channel GSP DefaultMain that uses the
 * ChannelGspRpcServer.
 */
class ChannelGspInstanceFactory : public CustomisedInstanceFactory
{

private:

  /**
   * Reference to the ChannelGame instance, which is needed for the RPC server
   * construction.
   */
  ChannelGame& chGame;

public:

  explicit ChannelGspInstanceFactory (ChannelGame& chg)
    : chGame(chg)
  {}

  std::unique_ptr<RpcServerInterface> BuildRpcServer (
      Game& game, jsonrpc::AbstractServerConnector& conn) override;

};

} // namespace xaya

#endif // GAMECHANNEL_GSPRPC_HPP
