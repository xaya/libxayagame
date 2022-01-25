// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_CHANNELRPC_HPP
#define XAYASHIPS_CHANNELRPC_HPP

#include "channel.hpp"
#include "proto/boardmove.pb.h"
#include "rpc-stubs/shipschannelrpcserverstub.h"

#include <gamechannel/daemon.hpp>

#include <json/json.h>
#include <jsonrpccpp/server.h>

#include <string>

namespace ships
{

/**
 * RPC server for the ships channel daemon interface.
 */
class ShipsChannelRpcServer : public ShipsChannelRpcServerStub
{

private:

  /** The ships channel data for RPC processing.  */
  ShipsChannel& channel;

  /**
   * The ChannelDaemon instance to use for RPC processing.  This by itself
   * also exposes the underlying ChannelManager.
   */
  xaya::ChannelDaemon& daemon;

  /**
   * Extends a given state JSON by extra data from the ShipsChannel directly
   * (i.e. the player's own position if set).
   */
  Json::Value ExtendStateJson (Json::Value&& state) const;

  /**
   * Processes a local move given as proto.  When this method gets called,
   * we already hold the lock on the channel manager, and pass the instance
   * in directly.
   */
  static void ProcessLocalMove (xaya::ChannelManager& cm,
                                const proto::BoardMove& mv);

public:

  explicit ShipsChannelRpcServer (ShipsChannel& c, xaya::ChannelDaemon& d,
                                  jsonrpc::AbstractServerConnector& conn)
    : ShipsChannelRpcServerStub(conn), channel(c), daemon(d)
  {}

  void stop () override;
  Json::Value getcurrentstate () override;
  Json::Value waitforchange (int knownVersion) override;

  void setposition (const std::string& str) override;
  void shoot (int column, int row) override;
  void revealposition () override;
  std::string filedispute () override;

  bool validateposition (const std::string& str) override;

};

} // namespace ships

#endif // XAYASHIPS_CHANNELRPC_HPP
