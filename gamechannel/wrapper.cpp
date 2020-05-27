// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wrapper.hpp"

#include <glog/logging.h>

namespace xaya
{

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

} // namespace xaya
