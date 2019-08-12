// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chaintochannel.hpp"

#include "protoutils.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayautil/base64.hpp>

#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>

#include <glog/logging.h>

namespace xaya
{

ChainToChannelFeeder::ChainToChannelFeeder (ChannelGspRpcClient& rBlocks,
                                            ChannelGspRpcClient* rPending,
                                            ChannelManager& cm)
  : rpcBlocks(rBlocks), rpcPending(rPending), manager(cm),
    channelIdHex(manager.GetChannelId ().ToHex ())
{
  lastBlock.SetNull ();
}

ChainToChannelFeeder::~ChainToChannelFeeder ()
{
  Stop ();
  CHECK (loopBlocks == nullptr);
  CHECK (loopPending == nullptr);
}

namespace
{

/**
 * Decodes a JSON value (which must be a base64-encoded string) into a
 * protocol buffer instance of the given type.
 */
template <typename Proto>
  Proto
  DecodeProto (const Json::Value& val)
{
  CHECK (val.isString ());

  Proto res;
  CHECK (ProtoFromBase64 (val.asString (), res));

  return res;
}

} // anonymous namespace

void
ChainToChannelFeeder::UpdateBlocks ()
{
  const auto data = rpcBlocks.getchannel (channelIdHex);

  if (data["state"] != "up-to-date")
    {
      LOG (WARNING)
          << "Channel GSP is in state " << data["state"]
          << ", not updating channel";
      return;
    }

  const auto& newBlockVal = data["blockhash"];
  if (newBlockVal.isNull ())
    {
      /* This will typically not happen, since we already check the return
         value of waitforchange.  But there are two situations where we could
         get here:  On the initial update, and (very unlikely) if the
         existing state gets detached between the waitforchange call and when
         we call getchannel.  */
      LOG (WARNING) << "GSP has no current state yet";
      return;
    }
  CHECK (newBlockVal.isString ());
  CHECK (lastBlock.FromHex (newBlockVal.asString ()));

  const auto& heightVal = data["height"];
  CHECK (heightVal.isUInt ());
  const unsigned height = heightVal.asUInt ();

  LOG (INFO)
      << "New on-chain best block: " << lastBlock.ToHex ()
      << " at height " << height;

  const auto& channel = data["channel"];
  if (channel.isNull ())
    {
      LOG (INFO) << "Channel " << channelIdHex << " is not known on-chain";
      manager.ProcessOnChainNonExistant (lastBlock, height);
      return;
    }
  CHECK (channel.isObject ());

  CHECK_EQ (channel["id"].asString (), channelIdHex);
  const auto meta
      = DecodeProto<proto::ChannelMetadata> (channel["meta"]["proto"]);
  const auto proof = DecodeProto<proto::StateProof> (channel["state"]["proof"]);

  BoardState reinitState;
  CHECK (DecodeBase64 (channel["reinit"]["base64"].asString (), reinitState));

  unsigned disputeHeight = 0;
  const auto& disputeVal = channel["disputeheight"];
  if (!disputeVal.isNull ())
    {
      CHECK (disputeVal.isUInt ());
      disputeHeight = disputeVal.asUInt ();
    }

  manager.ProcessOnChain (lastBlock, height, meta, reinitState,
                          proof, disputeHeight);
  LOG (INFO) << "Updated channel from on-chain state: " << channelIdHex;
}

void
ChainToChannelFeeder::RunBlockLoop ()
{
  UpdateBlocks ();

  while (!stopLoop)
    {
      const std::string lastBlockHex = lastBlock.ToHex ();

      std::string newBlockHex;
      try
        {
          newBlockHex = rpcBlocks.waitforchange (lastBlockHex);
        }
      catch (const jsonrpc::JsonRpcException& exc)
        {
          /* Especially timeouts are fine, we should just ignore them.
             A relatively small timeout is needed in order to not block
             too long when waiting for shutting down the loop.  */
          VLOG (1) << "Error calling waitforchange: " << exc.what ();
          CHECK_EQ (exc.GetCode (), jsonrpc::Errors::ERROR_CLIENT_CONNECTOR);
          continue;
        }

      if (newBlockHex.empty ())
        {
          VLOG (1) << "GSP does not have any state yet";
          continue;
        }

      if (newBlockHex == lastBlockHex)
        {
          VLOG (1) << "We are already at newest block";
          continue;
        }

      UpdateBlocks ();
    }
}

void
ChainToChannelFeeder::UpdatePending (const Json::Value& state)
{
  const auto& versionVal = state["version"];
  CHECK (versionVal.isInt ());
  pendingVersion = versionVal.asInt ();

  const auto& pending = state["pending"];
  CHECK (pending.isObject ());
  const auto& channels = pending["channels"];
  CHECK (channels.isObject ());

  const auto& ch = channels[channelIdHex];
  if (ch.isNull ())
    return;
  CHECK (ch.isObject ());

  const auto& proofVal = ch["proof"];
  CHECK (proofVal.isString ());
  if (lastPendingProof == proofVal.asString ())
    return;

  LOG (INFO)
      << "Detected new StateProof in pending move, turn count: "
      << ch["turncount"].asInt ();
  VLOG (2) << "Full pending channel update: " << ch;

  const auto proof = DecodeProto<proto::StateProof> (proofVal);
  manager.ProcessPending (proof);
  lastPendingProof = proofVal.asString ();
}

void
ChainToChannelFeeder::RunPendingLoop ()
{
  if (rpcPending == nullptr)
    {
      LOG (WARNING) << "Processing of pending moves is disabled";
      return;
    }

  try
    {
      UpdatePending (rpcPending->getpendingstate ());
    }
  catch (const jsonrpc::JsonRpcException& exc)
    {
      /* If this exception was thrown because processing of pending updates
         is disabled, then all is fine and we just stop the loop silently.  */
      LOG (INFO) << "Error calling getpendingstate: " << exc.what ();
      CHECK_EQ (exc.GetCode (), jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR);

      LOG (WARNING)
          << "Pending moves are disabled in the GSP,"
             " no updates will be processed";
      return;
    }

  while (!stopLoop)
    {
      try
        {
          UpdatePending (rpcPending->waitforpendingchange (pendingVersion));
        }
      catch (const jsonrpc::JsonRpcException& exc)
        {
          /* Especially timeouts are fine, we should just ignore them.
             A relatively small timeout is needed in order to not block
             too long when waiting for shutting down the loop.  */
          VLOG (1) << "Error calling waitforpendingchange: " << exc.what ();
          CHECK_EQ (exc.GetCode (), jsonrpc::Errors::ERROR_CLIENT_CONNECTOR);
          continue;
        }
    }
}

void
ChainToChannelFeeder::Start ()
{
  LOG (INFO) << "Starting chain-to-channel feeder loop...";
  CHECK (loopBlocks == nullptr) << "Feeder loop is already running";
  CHECK (loopPending == nullptr);

  stopLoop = false;
  loopBlocks = std::make_unique<std::thread> ([this] ()
    {
      RunBlockLoop ();
    });
  loopPending = std::make_unique<std::thread> ([this] ()
    {
      RunPendingLoop ();
    });
}

void
ChainToChannelFeeder::Stop ()
{
  /* We always either have both loops started or none.  If pending moves
     are disabled on the GSP, then the pending loop exits early; but the
     thread instance is still set.  */
  if (loopBlocks == nullptr)
    {
      CHECK (loopPending == nullptr);
      return;
    }
  CHECK (loopPending != nullptr);

  LOG (INFO) << "Stopping chain-to-channel feeder loop...";

  stopLoop = true;
  loopBlocks->join ();
  loopBlocks.reset ();
  loopPending->join ();
  loopPending.reset ();
}

} // namespace xaya
