// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_BOARDRULES_HPP
#define GAMECHANNEL_BOARDRULES_HPP

#include "proto/metadata.pb.h"
#include "protoversion.hpp"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <memory>
#include <string>

namespace xaya
{

class BoardRules;

/**
 * The state of the current game board, encoded in a game-specific format.
 * We use std::string simply as convenient wrapper for arbitrary data.
 */
using BoardState = std::string;

/** The game-specific encoded data of a move in a game channel.  */
using BoardMove = std::string;

/**
 * Interface for a game-specific "parsed" representation of a board state.
 * Instances of subclasses are obtained by parsing an (encoded) BoardState
 * through the game's BoardRules instance, and then those instances can be
 * used to further work with a game state.
 *
 * A typical usage pattern here is that the BoardState could be a serialised
 * protocol buffer, while the ParsedBoardState is a wrapper class around
 * the actual protocol buffer.
 */
class ParsedBoardState
{

private:

  /** A reference to the associated BoardRules instance.  */
  const BoardRules& rules;

  /**
   * A reference to the channel ID.  This is stored in the constructor and
   * can be accessed by subclasses.
   */
  const uint256& channelId;

  /**
   * A reference to the channel's metadata.  This is stored in the constructor
   * (from the BoardRules instance) and can be accessed by subclasses as
   * they need it for the game logic.  The reference originally comes from
   * BoardRules::ParseState, by which it is guaranteed to be valid as long
   * as the instance of the parsed state is there.
   */
  const proto::ChannelMetadata& meta;

protected:

  explicit ParsedBoardState (const BoardRules& r,
                             const uint256& id, const proto::ChannelMetadata& m)
    : rules(r), channelId(id), meta(m)
  {}

public:

  /**
   * Participant index value that indicates that it is currently no
   * player's turn in a game.  This is the case e.g. when the channel is
   * still waiting for players to join, or when the game has ended.
   */
  static constexpr int NO_TURN = -1;

  virtual ~ParsedBoardState () = default;

  /**
   * Returns the associated BoardRules instance.
   */
  const BoardRules&
  GetBoardRules () const
  {
    return rules;
  }

  /**
   * Returns the channel ID.
   */
  const uint256&
  GetChannelId () const
  {
    return channelId;
  }

  /**
   * Returns the metadata associated with this channel state.
   */
  const proto::ChannelMetadata&
  GetMetadata () const
  {
    return meta;
  }

  /**
   * Compares the current state to the given other board state.  Returns true
   * if they are equivalent (i.e. possibly different encodings of the same
   * state).
   *
   * The passed in BoardState may be invalid (even malformed encoded data),
   * in which case this function should return false.
   */
  virtual bool Equals (const BoardState& other) const = 0;

  /**
   * Returns which player's turn it is in the current state.  The return value
   * is the player index into the associated channel's participants array.
   * This may return NO_TURN to indicate that it is noone's turn at the moment.
   */
  virtual int WhoseTurn () const = 0;

  /**
   * Returns the "turn count" for the current game state.  This is a number
   * that should increase with turns made in the game, so that it is possible
   * to determine whether a given state is "after" another.  It can also be
   * seen as the "block height" in the "private chain" formed during a game
   * on a channel.
   */
  virtual unsigned TurnCount () const = 0;

  /**
   * Applies a move (assumed to be made by the player whose turn it is)
   * onto the current state, yielding a new board state.  Returns false
   * if the move is invalid instead (either because the data itself does
   * not represent a move at all, or because the move is invalid in the
   * context of the given old state).
   */
  virtual bool ApplyMove (XayaRpcClient& rpc, const BoardMove& mv,
                          BoardState& newState) const = 0;

  /**
   * Returns a JSON representation of the current board state.  This is used
   * by the game-channels daemons when communicating to frontends.  The default
   * implementation simply returns JSON null.  If there is a useful
   * representation of the state for the frontend, subclasses may override
   * the method accordingly to return more information.
   *
   * Note that the full board state is always returned as encoded data as well,
   * independently of what this method does.
   */
  virtual Json::Value ToJson () const;

};

/**
 * Abstract base class for the game-specific processor of board states and
 * moves on a channel.  This is the main class defining the rules of the
 * on-chain game, by means of constructing proper subclasses of
 * ParsedBoardState (which then do the real processing).
 */
class BoardRules
{

protected:

  BoardRules () = default;

public:

  virtual ~BoardRules () = default;

  /**
   * Parses an encoded BoardState into a ParsedBoardState instance, which
   * implements the abstract methods suitably for the game at hand.
   *
   * If the state is invalid (e.g. malformed data), this function should return
   * nullptr instead.
   *
   * The passed-in ID and metadata can be used to put the board state into
   * context.  It is guaranteed that the references stay valid at least as
   * long as the returned ParsedBoardState instance will be kept alive.
   */
  virtual std::unique_ptr<ParsedBoardState> ParseState (
      const uint256& channelId, const proto::ChannelMetadata& meta,
      const BoardState& s) const = 0;

  /**
   * Returns the version to apply for StateProof protos when a channel has
   * the given metadata.
   */
  virtual ChannelProtoVersion GetProtoVersion (
      const proto::ChannelMetadata& meta) const = 0;

};

} // namespace xaya

#endif // GAMECHANNEL_BOARDRULES_HPP
