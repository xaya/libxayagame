// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_BOARD_HPP
#define XAYASHIPS_BOARD_HPP

#include "proto/boardmove.pb.h"
#include "proto/boardstate.pb.h"

#include <gamechannel/protoboard.hpp>
#include <gamechannel/proto/metadata.pb.h>
#include <xayagame/rpc-stubs/xayarpcclient.h>

namespace ships
{

/** The base ProtoBoardState for our types.  */
using BaseProtoBoardState
    = xaya::ProtoBoardState<proto::BoardState, proto::BoardMove>;

/**
 * The main implementation of the ships board rules.
 *
 * Note that these rules apply only for channels with both participants already.
 * While a channel is waiting for the second player to join, no board functions
 * are invoked at all (as disputes / resolutions are not allowed by the core
 * logic itself).
 */
class ShipsBoardState : public BaseProtoBoardState
{

private:

  /** The current "phase" that the game is in according to a board state.  */
  enum class Phase
  {

    /** The proto is inconsistent and no phase can be determined.  */
    INVALID,

    /** The first player should commit their position and random seed.  */
    FIRST_COMMITMENT,

    /** The second player should commit their position.  */
    SECOND_COMMITMENT,

    /**
     * The first player should reveal their random seed to determine who
     * is the starting player.
     */
    FIRST_REVEAL_SEED,

    /** Ordinary game play:  A shot should be made.  */
    SHOOT,

    /** Ordinary game play:  A shot should be answered.  */
    ANSWER,

    /**
     * One player revealed the configuration, and now the second player has
     * to do so as well.
     */
    SECOND_REVEAL_POSITION,

    /** The game is finished and a winning player determined.  */
    WINNER_DETERMINED,

    /** The game is finished and the winner statement provided already.  */
    FINISHED,

  };

  /**
   * Determines the current "phase" of the game according to the proto we have.
   * The phase is implicit, based on what proto fields are set; this function
   * looks at those and returns the current phase or INVALID if the proto
   * state is inconsistent in any way.
   */
  Phase GetPhase () const;

  /**
   * Applies a position commitment move (if valid).
   */
  static bool ApplyPositionCommitment (const proto::PositionCommitmentMove& mv,
                                       const Phase phase,
                                       proto::BoardState& newState);

  /**
   * Applies a seed-reveal move (if valid).
   */
  static bool ApplySeedReveal (const proto::SeedRevealMove& mv,
                               const Phase phase, proto::BoardState& newState);

  friend class BoardTests;

protected:

  bool ApplyMoveProto (XayaRpcClient& rpc, const proto::BoardMove& mv,
                       proto::BoardState& newState) const override;

public:

  using BaseProtoBoardState::BaseProtoBoardState;

  bool IsValid () const override;
  int WhoseTurn () const override;
  unsigned TurnCount () const override;

};

/** The BoardRules instance we use for the ships game.  */
using ShipsBoardRules = xaya::ProtoBoardRules<ShipsBoardState>;

} // namespace ships

#endif // XAYASHIPS_BOARD_HPP
