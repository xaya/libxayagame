// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_CHANNEL_HPP
#define XAYASHIPS_CHANNEL_HPP

#include "board.hpp"
#include "coord.hpp"
#include "grid.hpp"

#include "proto/boardmove.pb.h"

#include <gamechannel/boardrules.hpp>
#include <gamechannel/openchannel.hpp>
#include <gamechannel/movesender.hpp>
#include <gamechannel/proto/stateproof.pb.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayautil/cryptorand.hpp>
#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <cstdint>
#include <string>

namespace ships
{

/**
 * Ships-specific data and logic for an open channel the player is involved
 * in.  This mostly takes care of the various commit-reveal schemes.
 */
class ShipsChannel : public xaya::OpenChannel
{

private:

  /** RPC connection to the wallet (for signing winner statements).  */
  XayaWalletRpcClient& wallet;

  /** The player name who is running this channel daemon.  */
  const std::string playerName;

  /** Generator for random salt values.  */
  xaya::CryptoRand rnd;

  /** The position of this player.  */
  Grid position;

  /** Salt for the position hash.  */
  xaya::uint256 positionSalt;

  /**
   * If this channel corresponds to the first player, then we save the
   * seed for determining the initial player here.
   */
  xaya::uint256 seed0;

  /**
   * Returns the index that the current player has for the given state.
   */
  int GetPlayerIndex (const ShipsBoardState& state) const;

  /**
   * Tries to do a position-commitment move (either for the first or second
   * player) as automove.  Returns true if possible (i.e. we have a position).
   */
  bool AutoPositionCommitment (proto::BoardMove& mv);

  /**
   * Real implementation of MaybeAutoMove, for which the conversion to
   * ShipsBoardState and between the proto and BoardMove is taken care of.
   */
  bool InternalAutoMove (const ShipsBoardState& state, proto::BoardMove& mv);

public:

  explicit ShipsChannel (XayaWalletRpcClient& w, const std::string& nm)
    : wallet(w), playerName(nm)
  {}

  ShipsChannel (const ShipsChannel&) = delete;
  void operator= (const ShipsChannel&) = delete;

  Json::Value ResolutionMove (const xaya::uint256& channelId,
                              const xaya::proto::StateProof& p) const override;
  Json::Value DisputeMove (const xaya::uint256& channelId,
                           const xaya::proto::StateProof& p) const override;

  bool MaybeAutoMove (const xaya::ParsedBoardState& state,
                      xaya::BoardMove& mv) override;
  void MaybeOnChainMove (const xaya::ParsedBoardState& state,
                         xaya::MoveSender& sender) override;

  /**
   * Returns true if the position has already been initialised.
   */
  bool IsPositionSet () const;

  /**
   * Returns the player's position (must be set for this to be valid).
   */
  const Grid& GetPosition () const;

  /**
   * Sets the player's position from the given Grid if it is valid.
   * Must not be called if IsPositionSet() is already true.
   */
  void SetPosition (const Grid& g);

  /**
   * Returns a ShotMove for the given target coordinate.
   */
  proto::BoardMove GetShotMove (const Coord& c) const;

  /**
   * Returns the move for revealing the player's position.  This is sent as
   * auto move if the other player revealed already or if all of their ships
   * have been hit, but it may also be used explicitly if the player
   * requests a revelation because they suspect fraud.
   *
   * This must only be called if IsPositionSet() is true.
   */
  proto::BoardMove GetPositionRevealMove () const;

};

} // namespace ships

#endif // XAYASHIPS_CHANNEL_HPP
