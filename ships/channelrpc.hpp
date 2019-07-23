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

#include <mutex>
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
   * Mutex for synchronising access particularly to the ShipsChannel.
   * The ChannelManager has its own synchronisation in place for processing
   * updates, but the RPC server uses the ShipsChannel instance before
   * passing any updates to ChannelManager.  Thus we need our own lock
   * with a  "wider scope".
   */
  mutable std::mutex mut;

  /**
   * Processes a local move given as proto.
   */
  void ProcessLocalMove (const proto::BoardMove& mv);

  /**
   * Extends a given state JSON by extra data from the ShipsChannel directly
   * (i.e. the player's own position if set).
   */
  Json::Value ExtendStateJson (Json::Value&& state) const;

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

};

} // namespace ships

#endif // XAYASHIPS_CHANNELRPC_HPP
