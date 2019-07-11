// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_OPENCHANNEL_HPP
#define GAMECHANNEL_OPENCHANNEL_HPP

#include "proto/stateproof.pb.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>

namespace xaya
{

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

};

} // namespace xaya

#endif // GAMECHANNEL_OPENCHANNEL_HPP
