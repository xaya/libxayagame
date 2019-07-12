// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_RPCBROADCAST_HPP
#define GAMECHANNEL_RPCBROADCAST_HPP

#include "broadcast.hpp"
#include "channelmanager.hpp"

#include "rpc-stubs/rpcbroadcastclient.h"

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <string>
#include <vector>

namespace xaya
{

/**
 * Implementation of OffChainBroadcast that talks to a JSON-RPC server
 * for sending and receiving messages.  The server manages the individual
 * channels and takes care of distributing the messages to clients.
 */
class RpcBroadcast : public OffChainBroadcast
{

private:

  /** The HTTP connector used for sending messages.  */
  jsonrpc::HttpClient sendConnector;

  /**
   * The HTTP connector used for receiving messages.  We need a separate one
   * here from sendConnector, because both may be used concurrently by
   * different threads and that is not possible with a single one.
   */
  jsonrpc::HttpClient receiveConnector;

  /** The RPC client used for sending messages.  */
  RpcBroadcastClient sendRpc;

  /** The RPC client used for receiving messages.  */
  RpcBroadcastClient receiveRpc;

  /** The last known sequence number of the channel.  */
  unsigned seq;

  /**
   * Initialises the sequence number from the RPC server.
   */
  void InitialiseSequence ();

  /**
   * Updates the internal sequence number from a received response (either
   * for getseq or receive).
   */
  void UpdateSequence (const Json::Value& resp);

  friend class TestRpcBroadcast;

protected:

  void SendMessage (const std::string& msg) override;
  std::vector<std::string> GetMessages () override;

public:

  explicit RpcBroadcast (const std::string& rpcUrl, ChannelManager& cm);

  /**
   * Starts the broadcast channel.  We use the default event loop, but override
   * this method to query for the initial sequence number when starting.
   */
  void Start () override;

};

} // namespace xaya

#endif // GAMECHANNEL_BROADCAST_HPP
