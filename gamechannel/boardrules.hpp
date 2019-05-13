// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_BOARDRULES_HPP
#define GAMECHANNEL_BOARDRULES_HPP

#include "proto/metadata.pb.h"

#include <string>

namespace xaya
{

/**
 * The state of the current game board, encoded in a game-specific format.
 * We use std::string simply as convenient wrapper for arbitrary data.
 */
using BoardState = std::string;

/** The game-specific encoded data of a move in a game channel.  */
using BoardMove = std::string;

/**
 * Abstract base class for the game-specific processor of board states and
 * moves on a channel.  This is the main class defining the rules of the
 * on-chain game.
 *
 * The implemented methods of this class should be pure and thread-safe.
 * They may be called in parallel and from various different threads from
 * the game-channel framework.
 */
class BoardRules
{

protected:

  BoardRules () = default;

public:

  /**
   * Participant index value that indicates that it is currently no
   * player's turn in a game.  This is the case e.g. when the channel is
   * still waiting for players to join, or when the game has ended.
   */
  static constexpr int NO_TURN = -1;

  virtual ~BoardRules () = default;

  /**
   * Compares two given board states in the context of the given metadata.
   * Returns true if they are equivalent (i.e. possibly different encodings
   * of the same state).
   *
   * If one or both of the passed states is invalid (e.g. malformed data),
   * then the function should return false.
   */
  virtual bool CompareStates (const proto::ChannelMetadata& meta,
                              const BoardState& a,
                              const BoardState& b) const = 0;

  /**
   * Returns which player's turn it is in the given state.  The return value
   * is the player index into the channel's participants array.  This may return
   * NO_TURN to indicate that it is noone's turn at the moment.
   *
   * It is guaranteed that this function is never called on an invalid state.
   * In other words, it is only called on results of successful calls
   * to ApplyMove.
   */
  virtual int WhoseTurn (const proto::ChannelMetadata& meta,
                         const BoardState& state) const = 0;

  /**
   * Returns the "turn count" for the given game state.  This is a number
   * that should increase with turns made in the game, so that it is possible
   * to determine whether a given state is "after" another.  It can also be
   * seen as the "block height" in the "private chain" formed during a game
   * on a channel.
   *
   * It is guaranteed that this function is never called on an invalid state.
   * In other words, it is only called on results of successful calls
   * to ApplyMove.
   */
  virtual unsigned TurnCount (const proto::ChannelMetadata& meta,
                              const BoardState& state) const = 0;

  /**
   * Applies a move (assumed to be made by the player whose turn it is)
   * onto the given state, yielding a new board state.  Returns false
   * if the move is invalid instead (either because the data itself does
   * not represent a move at all, or because the move is invalid in the
   * context of the given old state).
   *
   * If the old state is invalid (e.g. malformed data), then this function
   * should also return false.
   */
  virtual bool ApplyMove (const proto::ChannelMetadata& meta,
                          const BoardState& oldState, const BoardMove& mv,
                          BoardState& newState) const = 0;

};

} // namespace xaya

#endif // GAMECHANNEL_BOARDRULES_HPP
