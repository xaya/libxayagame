// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcbroadcast.hpp"

#include <xayautil/base64.hpp>

#include <glog/logging.h>

namespace xaya
{

RpcBroadcast::RpcBroadcast (const std::string& rpcUrl,
                            SynchronisedChannelManager& cm)
  : ReceivingOffChainBroadcast(cm),
    sendConnector(rpcUrl), receiveConnector(rpcUrl),
    sendRpc(sendConnector), receiveRpc(receiveConnector)
{}

RpcBroadcast::RpcBroadcast (const std::string& rpcUrl, const uint256& id)
  : ReceivingOffChainBroadcast(id),
    sendConnector(rpcUrl), receiveConnector(rpcUrl),
    sendRpc(sendConnector), receiveRpc(receiveConnector)
{}

void
RpcBroadcast::InitialiseSequence ()
{
  LOG (INFO) << "Querying RPC server for initial sequence number...";
  UpdateSequence (receiveRpc.getseq (GetChannelId ().ToHex ()));
}

void
RpcBroadcast::UpdateSequence (const Json::Value& resp)
{
  CHECK (resp.isObject ());
  const auto& seqVal = resp["seq"];
  CHECK (seqVal.isUInt ());
  seq = seqVal.asUInt ();
  VLOG (1) << "New sequence number: " << seq;
}

void
RpcBroadcast::Start ()
{
  InitialiseSequence ();
  ReceivingOffChainBroadcast::Start ();
}

void
RpcBroadcast::SendMessage (const std::string& msg)
{
  /* While going through the RPC server, we encode messages as base64 to
     ensure that they can safely and easily be transmitted through JSON.  */
  sendRpc.send (GetChannelId ().ToHex (), EncodeBase64 (msg));
}

std::vector<std::string>
RpcBroadcast::GetMessages ()
{
  const Json::Value res = receiveRpc.receive (GetChannelId ().ToHex (), seq);
  CHECK (res.isObject ());
  UpdateSequence (res);

  const auto& msgVal = res["messages"];
  CHECK (msgVal.isArray ());
  std::vector<std::string> messages;
  messages.reserve (msgVal.size ());
  for (const auto& m : msgVal)
    {
      CHECK (m.isString ());

      std::string decoded;
      if (!DecodeBase64 (m.asString (), decoded))
        {
          LOG (WARNING)
              << "Invalid base64 detected in broadcast message: " << m;
          continue;
        }

      messages.push_back (decoded);
    }

  return messages;
}

} // namespace xaya
