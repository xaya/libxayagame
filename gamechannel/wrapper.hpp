// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_WRAPPER_HPP
#define GAMECHANNEL_WRAPPER_HPP

#include "boardrules.hpp"

#include "proto/metadata.pb.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <memory>
#include <string>

namespace xaya
{

/**
 * A struct with function pointers that define all the logic required
 * to implement a full BoardRules instance.  They operate on byte strings
 * as state and move, and should implement their own serialisation and
 * deserialisation logic for them.
 *
 * The channel ID is passed as hex string, and the channel metadata as
 * binary serialised protocol buffer.
 */
struct BoardRulesCallbacks
{

  /**
   * Verifies if the given state is actually valid according to the
   * internal format.
   */
  bool (*IsStateValid) (const BoardState& state);

  /**
   * Returns true if the two states are equal (but possibly in a different
   * encoding).
   */
  bool (*StatesEqual) (const BoardState& a, const BoardState& b);

  /**
   * Returns the player index whose turn it is.  Should return
   * ParsedBoardState::NO_TURN if it is noone's turn, e.g. because
   * the channel is waiting for more people to join.
   */
  int (*WhoseTurn) (const std::string& metadata, const BoardState& state);

  /**
   * Returns the turn count for the given state.
   */
  unsigned (*TurnCount) (const std::string& metadata, const BoardState& state);

  /**
   * Applies a move (assumed to be made by the player whose turn it is)
   * onto the current state, yielding a new board state.  Returns false
   * if the move is invalid instead.
   */
  bool (*ApplyMove) (const std::string& channelId, const std::string& metadata,
                     const BoardState& state, const BoardMove& mv,
                     BoardState& newState);

};

/**
 * An implementation of BoardRules based on callback functions.  This can
 * be used to define board rules without explicitly creating subclasses,
 * for instance to use in binding another language to the library.
 */
class CallbackBoardRules : public BoardRules
{

private:

  /** The callbacks used.  */
  const BoardRulesCallbacks cb;

public:

  /**
   * Constructs a set of board rules with the given callbacks.
   */
  explicit CallbackBoardRules (const BoardRulesCallbacks& c)
    : cb(c)
  {}

  std::unique_ptr<ParsedBoardState> ParseState (
      const uint256& channelId, const proto::ChannelMetadata& meta,
      const BoardState& state) const override;

  ChannelProtoVersion GetProtoVersion (
      const proto::ChannelMetadata& meta) const override;

};

} // namespace xaya

#endif // GAMECHANNEL_WRAPPER_HPP
