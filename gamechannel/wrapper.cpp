// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wrapper.hpp"

#include "daemon.hpp"
#include "movesender.hpp"
#include "rpcbroadcast.hpp"

#include "rpc-stubs/wrappedchannelserverstub.h"

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <glog/logging.h>

namespace xaya
{

/* ************************************************************************** */

namespace
{

/**
 * The implementation of ParsedBoardState that we use for CallbackBoardRules.
 */
class CallbackParsedState : public ParsedBoardState
{

private:

  /** The callbacks to use.  */
  const BoardRulesCallbacks& cb;

  /** The underlying state data (in user-defined format).  */
  const BoardState state;

  /** The channel ID as hex string.  */
  std::string channelIdHex;

  /** The metadata protocol buffer serialised as binary string.  */
  std::string metadataBytes;

public:

  explicit CallbackParsedState (const BoardRules& r, const uint256& id,
                                const proto::ChannelMetadata& m,
                                const BoardRulesCallbacks& c,
                                const BoardState& s)
    : ParsedBoardState(r, id, m), cb(c), state(s)
  {
    channelIdHex = GetChannelId ().ToHex ();
    CHECK (GetMetadata ().SerializeToString (&metadataBytes));
  }

  /**
   * Returns the actual state.
   */
  const BoardState&
  GetState () const
  {
    return state;
  }

  bool
  Equals (const BoardState& other) const override
  {
    return cb.StatesEqual (state, other);
  }

  int
  WhoseTurn () const override
  {
    return cb.WhoseTurn (metadataBytes, state);
  }

  unsigned
  TurnCount () const override
  {
    return cb.TurnCount (metadataBytes, state);
  }

  bool
  ApplyMove (XayaRpcClient& rpc, const BoardMove& mv,
             BoardState& newState) const override
  {
    return cb.ApplyMove (channelIdHex, metadataBytes, state, mv, newState);
  }

  /**
   * Returns the associated JSON state, which is just the board state
   * as a JSON string.
   */
  Json::Value
  ToJson () const override
  {
    return state;
  }

};

} // anonymous namespace

std::unique_ptr<ParsedBoardState>
CallbackBoardRules::ParseState (
    const uint256& channelId, const proto::ChannelMetadata& meta,
    const BoardState& state) const
{
  if (!cb.IsStateValid (state))
    return nullptr;

  return std::make_unique<CallbackParsedState> (*this, channelId, meta,
                                                cb, state);
}

ChannelProtoVersion
CallbackBoardRules::GetProtoVersion (const proto::ChannelMetadata& meta) const
{
  return ChannelProtoVersion::ORIGINAL;
}

/* ************************************************************************** */

Json::Value
CallbackOpenChannel::ResolutionMove (const uint256& channelId,
                                     const proto::StateProof& proof) const
{
  std::string proofBytes;
  CHECK (proof.SerializeToString (&proofBytes));
  return cb.ResolutionMove (channelId.ToHex (), proofBytes);
}

Json::Value
CallbackOpenChannel::DisputeMove (const uint256& channelId,
                                  const proto::StateProof& proof) const
{
  std::string proofBytes;
  CHECK (proof.SerializeToString (&proofBytes));
  return cb.DisputeMove (channelId.ToHex (), proofBytes);
}

namespace
{

/**
 * Extracts and returns the underlying state, assuming the instance
 * is from a CallbackBoardRules.
 */
const BoardState&
ExtractBoardState (const ParsedBoardState& state)
{
  return dynamic_cast<const CallbackParsedState&> (state).GetState ();
}

} // anonymous namespace

bool
CallbackOpenChannel::MaybeAutoMove (const ParsedBoardState& state,
                                    BoardMove& mv)
{
  std::string meta;
  CHECK (state.GetMetadata ().SerializeToString (&meta));

  return cb.MaybeAutoMove (state.GetChannelId ().ToHex (), meta, playerName,
                           ExtractBoardState (state), priv, mv);
}

void
CallbackOpenChannel::MaybeOnChainMove (const ParsedBoardState& state,
                                       MoveSender& sender)
{
  std::string meta;
  CHECK (state.GetMetadata ().SerializeToString (&meta));

  Json::Value mv;
  if (cb.MaybeOnChainMove (state.GetChannelId ().ToHex (), meta, playerName,
                           ExtractBoardState (state), priv, mv))
    sender.SendMove (mv);
}

/* ************************************************************************** */

namespace
{

/**
 * The JSON-RPC server implementation for the callback-based channel daemon.
 */
class WrappedChannelRpcServer : public WrappedChannelServerStub
{

private:

  /** The underlying channel for RPC processing.  */
  CallbackOpenChannel& channel;

  /** The ChannelDaemon instance (including its ChannelManager).  */
  ChannelDaemon& daemon;

  /**
   * Extends a given state JSON by the private state from channel.
   */
  Json::Value ExtendStateJson (Json::Value&& state) const;

public:

  explicit WrappedChannelRpcServer (CallbackOpenChannel& c, ChannelDaemon& d,
                                    jsonrpc::AbstractServerConnector& conn)
    : WrappedChannelServerStub(conn), channel(c), daemon(d)
  {}

  void stop () override;
  Json::Value getcurrentstate () override;
  Json::Value waitforchange (int knownVersion) override;

  void sendlocalmove (const std::string& mv) override;
  void setprivatestate (const std::string& ps) override;

  std::string filedispute () override;

};

Json::Value
WrappedChannelRpcServer::ExtendStateJson (Json::Value&& state) const
{
  state["private"] = channel.GetPrivateState ();
  return state;
}

void
WrappedChannelRpcServer::stop ()
{
  LOG (INFO) << "Channel RPC method called: stop";
  daemon.RequestStop ();
}

Json::Value
WrappedChannelRpcServer::getcurrentstate ()
{
  LOG (INFO) << "Channel RPC method called: getcurrentstate";
  return ExtendStateJson (daemon.GetChannelManager ().ToJson ());
}

Json::Value
WrappedChannelRpcServer::waitforchange (const int knownVersion)
{
  LOG (INFO) << "Channel RPC method called: waitforchange " << knownVersion;
  Json::Value state = daemon.GetChannelManager ().WaitForChange (knownVersion);
  return ExtendStateJson (std::move (state));
}

void
WrappedChannelRpcServer::sendlocalmove (const std::string& mv)
{
  LOG (INFO) << "Channel RPC method called: sendlocalmove " << mv;
  daemon.GetChannelManager ().ProcessLocalMove (mv);
}

void
WrappedChannelRpcServer::setprivatestate (const std::string& ps)
{
  LOG (INFO) << "Channel RPC method called: setprivatestate " << ps;
  channel.SetPrivateState (ps);
  daemon.GetChannelManager ().TriggerAutoMoves ();
}

std::string
WrappedChannelRpcServer::filedispute ()
{
  LOG (INFO) << "Channel RPC method called: filedispute";
  const uint256 txid = daemon.GetChannelManager ().FileDispute ();

  if (txid.IsNull ())
    return "";

  return txid.ToHex ();
}

} // anonymous namespace

void
RunCallbackChannel (const CallbackChannelConfig& cfg)
{
  uint256 channelId;
  CHECK (channelId.FromHex (cfg.ChannelId))
      << "Invalid channel ID: " << cfg.ChannelId;

  CHECK (!cfg.PlayerName.empty ()) << "No player name specified";

  CallbackBoardRules rules(cfg.RuleCallbacks);
  CallbackOpenChannel channel(cfg.ChannelCallbacks, cfg.PlayerName, "");

  ChannelDaemon daemon(cfg.GameId, channelId, cfg.PlayerName, rules, channel);
  daemon.ConnectXayaRpc (cfg.XayaRpcUrl);
  daemon.ConnectGspRpc (cfg.GspRpcUrl);

  RpcBroadcast bc(cfg.BroadcastRpcUrl, daemon.GetChannelManager ());
  daemon.SetOffChainBroadcast (bc);

  std::unique_ptr<jsonrpc::AbstractServerConnector> serverConnector;
  if (cfg.ChannelRpcPort != 0)
    {
      auto srv = std::make_unique<jsonrpc::HttpServer> (cfg.ChannelRpcPort);
      srv->BindLocalhost ();
      serverConnector = std::move (srv);
    }

  std::unique_ptr<WrappedChannelRpcServer> rpcServer;
  if (serverConnector != nullptr)
    {
      rpcServer = std::make_unique<WrappedChannelRpcServer> (
                      channel, daemon, *serverConnector);
      rpcServer->StartListening ();
    }
  else
    LOG (WARNING) << "Channel daemon has no JSON-RPC interface";

  daemon.Run ();

  if (rpcServer != nullptr)
    rpcServer->StopListening ();
}

/* ************************************************************************** */

} // namespace xaya
