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
   */
  virtual bool CompareStates (const ChannelMetadata& meta,
                              const BoardState& a,
                              const BoardState& b) const = 0;

  /**
   * Returns which player's turn it is in the given state.  The return value
   * is the player index into the channel's participants array.  This may return
   * NO_TURN to indicate that it is noone's turn at the moment.
   */
  virtual int WhoseTurn (const ChannelMetadata& meta,
                         const BoardState& a) const = 0;

  /**
   * Applies a move (assumed to be made by the player whose turn it is)
   * onto the given state, yielding a new board state.  Returns false
   * if the move is invalid instead.
   */
  virtual bool ApplyMove (const ChannelMetadata& meta,
                          const BoardState& oldState, const BoardMove& mv,
                          BoardState& newState) const = 0;

};

} // namespace xaya

#endif // GAMECHANNEL_BOARDRULES_HPP
