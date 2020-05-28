// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wrapper.hpp"

#include "daemon.hpp"
#include "movesender.hpp"
#include "rpcbroadcast.hpp"

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

  daemon.Run ();
}

/* ************************************************************************** */

} // namespace xaya
