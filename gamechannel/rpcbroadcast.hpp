// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_RPCBROADCAST_HPP
#define GAMECHANNEL_RPCBROADCAST_HPP

#include "recvbroadcast.hpp"
#include "channelmanager.hpp"

#include "rpc-stubs/rpcbroadcastclient.h"

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <string>
#include <vector>

/* It seems that Windows systems define SendMessage as a macro, which will
   clash with our usage as member function.  The corresponding header is
   included by libjson-rpc-cpp (thus this is not an issue with the general
   broadcast interface).  It is obviously a bit of a hack to just #undef the
   macro here, but it works and is for now the simplest solution.  */
#undef SendMessage

namespace xaya
{

/**
 * Implementation of OffChainBroadcast that talks to a JSON-RPC server
 * for sending and receiving messages.  The server manages the individual
 * channels and takes care of distributing the messages to clients.
 */
class RpcBroadcast : public ReceivingOffChainBroadcast
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

  explicit RpcBroadcast (const std::string& rpcUrl, const uint256& id);

  void SendMessage (const std::string& msg) override;
  std::vector<std::string> GetMessages () override;

public:

  explicit RpcBroadcast (const std::string& rpcUrl,
                         SynchronisedChannelManager& cm);

  /**
   * Starts the broadcast channel.  We use the default event loop, but override
   * this method to query for the initial sequence number when starting.
   */
  void Start () override;

};

} // namespace xaya

#endif // GAMECHANNEL_BROADCAST_HPP
