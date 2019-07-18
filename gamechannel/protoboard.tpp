// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Template implementation code for protoboard.hpp.  */

#include <google/protobuf/util/message_differencer.h>

#include <glog/logging.h>

#include <string>

namespace xaya
{

template <typename State, typename Move>
  ProtoBoardState<State, Move>::ProtoBoardState (
      const uint256& id, const proto::ChannelMetadata& m, State&& s)
  : ParsedBoardState(id, m)
{
  state.Swap (&s);
}

template <typename State, typename Move>
  bool
  ProtoBoardState<State, Move>::EqualsProto (const State& other) const
{
  return google::protobuf::util::MessageDifferencer::Equals (state, other);
}

template <typename State, typename Move>
  bool
  ProtoBoardState<State, Move>::IsValid () const
{
  return true;
}

template <typename State, typename Move>
  bool
  ProtoBoardState<State, Move>::Equals (const BoardState& other) const
{
  State po;
  if (!po.ParseFromString (other))
    {
      LOG (WARNING) << "Other BoardState failed to parse, returning not equal";
      return false;
    }

  return EqualsProto (po);
}

template <typename State, typename Move>
  bool
  ProtoBoardState<State, Move>::ApplyMove (XayaRpcClient& rpc,
                                           const BoardMove& mv,
                                           BoardState& newState) const
{
  Move pm;
  if (!pm.ParseFromString (mv))
    {
      LOG (WARNING) << "Failed to parse BoardMove into protocol buffer";
      return false;
    }

  State pn;
  if (!ApplyMoveProto (rpc, pm, pn))
    return false;

  CHECK (pn.SerializeToString (&newState));
  return true;
}

template <typename StateClass>
  std::unique_ptr<ParsedBoardState>
  ProtoBoardRules<StateClass>::ParseState (
      const uint256& channelId, const proto::ChannelMetadata& meta,
      const BoardState& s) const
{
  typename StateClass::StateProto p;
  if (!p.ParseFromString (s))
    {
      LOG (WARNING) << "Failed to parse BoardState into protocol buffer";
      return nullptr;
    }

  auto res = std::make_unique<StateClass> (channelId, meta, std::move (p));
  if (!res->IsValid ())
    {
      LOG (WARNING) << "Parsed BoardState is invalid";
      return nullptr;
    }

  return res;
}

} // namespace xaya
