// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_PROTOBOARD_HPP
#define GAMECHANNEL_PROTOBOARD_HPP

#include "boardrules.hpp"

#include "proto/metadata.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>

#include <json/json.h>

#include <memory>

namespace xaya
{

/**
 * Implementation of a ParsedBoardState where the encoded state and move
 * are both protocol buffers.  This utility class takes care of encoding
 * and decoding the protocol buffers, while the actual computation logic
 * is still left to be implemented by a subclass.
 */
template <typename State, typename Move>
  class ProtoBoardState : public ParsedBoardState
{

private:

  /** The parsed state proto itself.  */
  State state;

protected:

  /**
   * Compares the state to another instance of the state proto.  By default,
   * this method just compares the protocol buffer instances using
   * MessageDifferencer::Equals.  Subclasses can override this as needed
   * if they need custom comparison criteria.
   *
   * Note that other may be an arbitrary proto message, it is not guaranteed
   * to be "valid" (as per IsValid).
   */
  virtual bool EqualsProto (const State& other) const;

  /**
   * Applies the given move to compute the resulting new state.  Both move
   * and newState are protocol buffers for this method.  This must be
   * implemented by subclasses instead of ApplyMove.
   */
  virtual bool ApplyMoveProto (XayaRpcClient& rpc, const Move& mv,
                               State& newState) const = 0;

public:

  /**
   * Define the type of the state proto, so it need not be duplicated for
   * ProtoBoardRules.
   */
  using StateProto = State;

  /**
   * Constructs this instance based on the given metadata and swapping in
   * the provided state proto.  This is mostly intended to be called from
   * ProtoBoardRules.
   */
  explicit ProtoBoardState (const BoardRules& r, const uint256& channelId,
                            const proto::ChannelMetadata& m, State&& s);

  ProtoBoardState () = delete;
  ProtoBoardState (const ProtoBoardState<State, Move>&) = delete;
  void operator= (const ProtoBoardState<State, Move>&) = delete;

  /**
   * Returns the protocol buffer representing the current state.
   */
  const State&
  GetState () const
  {
    return state;
  }

  /**
   * Returns whether the protocol buffer state represents an actually
   * valid board state.  By default, this function just returns true.  It can
   * be overwritten by subclasses if they need to perform more consistency
   * checks.
   */
  virtual bool IsValid () const;

  bool Equals (const BoardState& other) const override;
  bool ApplyMove (XayaRpcClient& rpc, const BoardMove& mv,
                  BoardState& newState) const override;

};

/**
 * Utility class that implements the BoardRules interface and creates
 * ProtoBoardState-subclasses by deserialising the state as protocol buffer.
 */
template <typename StateClass>
  class ProtoBoardRules : public BoardRules
{

public:

  ProtoBoardRules () = default;

  ProtoBoardRules (const ProtoBoardRules<StateClass>&) = delete;
  void operator= (const ProtoBoardRules<StateClass>&) = delete;

  std::unique_ptr<ParsedBoardState> ParseState (
      const uint256& channelId, const proto::ChannelMetadata& meta,
      const BoardState& s) const override;

};

} // namespace xaya

#include "protoboard.tpp"

#endif // GAMECHANNEL_PROTOBOARD_HPP
