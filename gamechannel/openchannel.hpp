// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_OPENCHANNEL_HPP
#define GAMECHANNEL_OPENCHANNEL_HPP

#include "boardrules.hpp"
#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>

namespace xaya
{

class MoveSender;

/**
 * Data that a game wants to store about a particular open channel the player
 * is taking part in.  This can hold state (e.g. preimages for hash commitments)
 * and also needs to implement game-specific functions like building dispute
 * moves and processing auto-moves.
 *
 * This class is an equivalent of GameLogic and BoardRules for managing
 * an open channel in the channel daemon process.
 */
class OpenChannel
{

protected:

  OpenChannel () = default;

public:

  virtual ~OpenChannel () = default;

  /**
   * Builds a resolution move (just the move data without the game ID
   * envelope) for the given state proof and channel.
   */
  virtual Json::Value ResolutionMove (const uint256& channelId,
                                      const proto::StateProof& proof) const = 0;

  /**
   * Builds a dispute move (just the move data without the game ID
   * envelope) for the given state proof and channel.
   */
  virtual Json::Value DisputeMove (const uint256& channelId,
                                   const proto::StateProof& proof) const = 0;

  /**
   * Checks if an automatic move can be sent right now for the given game
   * state.  This is useful for situations where moves are made according
   * to some protocol, e.g. for hash commitments and random numbers.  The
   * default implementation just returns false, i.e. indicating that auto
   * moves are never available.
   *
   * This function is not marked as "const", since it may change the internal
   * state of the game-specific data.  For instance, when computing the auto
   * move, the game might construct and save some random salt value for
   * a hash commitment.
   */
  virtual bool MaybeAutoMove (const ParsedBoardState& state, BoardMove& mv);

  /**
   * Checks if the game-specific logic wants to send an on-chain move in
   * response to the current channel state.  This can be used, for instance,
   * to close a channel in agreement after the off-chain game has finished.
   *
   * Note that this function is called independent of whose turn it is
   * (unlike auto moves, which are processed only if the player owning the
   * channel daemon is to play).
   */
  virtual void MaybeOnChainMove (const proto::ChannelMetadata& meta,
                                 const ParsedBoardState& state,
                                 MoveSender& sender);

};

} // namespace xaya

#endif // GAMECHANNEL_OPENCHANNEL_HPP
