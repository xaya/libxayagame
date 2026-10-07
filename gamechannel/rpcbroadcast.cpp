// Copyright (C) 2019-2026 The Xaya developers
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
  if (seqVal.isUInt ())
    {
      seq = seqVal.asUInt ();
      VLOG (1) << "New sequence number: " << seq;
    }
  else
    LOG (WARNING) << "Server returned invalid sequence number: " << resp;
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

  std::vector<std::string> messages;

  const auto& msgVal = res["messages"];
  if (msgVal.isArray ())
    {
      messages.reserve (msgVal.size ());
      for (const auto& m : msgVal)
        {
          std::string decoded;
          if (!m.isString () || !DecodeBase64 (m.asString (), decoded))
            {
              LOG (WARNING) << "Invalid broadcast message: " << m;
              continue;
            }

          messages.push_back (decoded);
        }
    }
  else
    LOG (WARNING) << "Invalid messages returned: " << res;

  return messages;
}

} // namespace xaya
