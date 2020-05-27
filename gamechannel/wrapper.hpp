// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_WRAPPER_HPP
#define GAMECHANNEL_WRAPPER_HPP

#include "boardrules.hpp"
#include "openchannel.hpp"

#include "proto/metadata.pb.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <memory>
#include <string>

namespace xaya
{

/* ************************************************************************** */

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

/* ************************************************************************** */

/** Private data of a channel in a user-specific encoded format.  */
using PrivateState = std::string;

/**
 * Callbacks implementing OpenChannel behaviour.  They handle the
 * game-specific move formats for disputes and resolutions, as well as
 * automatic on-chain and channel moves.
 *
 * The channel ID is passed as hex string, and state proofs are passed as
 * binary-serialised protocol buffer.
 */
struct OpenChannelCallbacks
{

  /**
   * Constructs the game-specific move format (without game-ID envelope)
   * for a resolution.
   */
  Json::Value (*ResolutionMove) (const std::string& channelId,
                                 const std::string& proof);

  /**
   * Constructs the game-specific move format (without game-ID envelope)
   * for a dispute.
   */
  Json::Value (*DisputeMove) (const std::string& channelId,
                              const std::string& proof);

  /**
   * Checks if an automatic move can be made right now for the given game
   * state, assuming it is the current player's turn.
   */
  bool (*MaybeAutoMove) (const std::string& channelId, const std::string& meta,
                         const std::string& playerName,
                         const BoardState& state, const PrivateState& priv,
                         BoardMove& mv);

  /**
   * Checks if an on-chain transaction should be made.  If this returns true,
   * then a move is sent by the player name with the given JSON data (wrapped
   * up together with the game ID).
   */
  bool (*MaybeOnChainMove) (const std::string& channelId,
                            const std::string& meta,
                            const std::string& playerName,
                            const BoardState& state, const PrivateState& priv,
                            Json::Value& mv);

};

/**
 * Implementation of OpenChannel based on a set of callbacks.  It holds
 * also a user-defined private state as string of arbitrary bytes, which
 * can be used by the OpenChannel callbacks in a game-specific way.
 *
 * This class must only be used together with CallbackBoardRules, as it
 * assumes the underlying type of ParsedBoardState!
 */
class CallbackOpenChannel : public OpenChannel
{

private:

  /** The callbacks used for the implementation.  */
  const OpenChannelCallbacks cb;

  /** The name of the user playing this channel.  */
  const std::string playerName;

  /** The current private state of the channel.  */
  PrivateState priv;

public:

  explicit CallbackOpenChannel (const OpenChannelCallbacks& c,
                                const std::string& nm, const PrivateState& ps)
    : cb(c), playerName(nm), priv(ps)
  {}

  Json::Value ResolutionMove (const uint256& channelId,
                              const proto::StateProof& proof) const override;
  Json::Value DisputeMove (const uint256& channelId,
                           const proto::StateProof& proof) const override;

  bool MaybeAutoMove (const ParsedBoardState& state, BoardMove& mv) override;
  void MaybeOnChainMove (const ParsedBoardState& state,
                         MoveSender& sender) override;

};

/* ************************************************************************** */

} // namespace xaya

#endif // GAMECHANNEL_WRAPPER_HPP
