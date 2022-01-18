// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_BOARD_HPP
#define XAYASHIPS_BOARD_HPP

#include "proto/boardmove.pb.h"
#include "proto/boardstate.pb.h"

#include <gamechannel/protoboard.hpp>
#include <gamechannel/proto/metadata.pb.h>

#include <json/json.h>

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

    /** The game is finished and the winner determined.  */
    FINISHED,

  };

  /**
   * Determines the current "phase" of the game according to the proto we have.
   * The phase is implicit, based on what proto fields are set; this function
   * looks at those and returns the current phase or INVALID if the proto
   * state is inconsistent in any way.
   */
  Phase GetPhase () const;

  /* Helper routines that apply a given move to the state, modifying it
     in-place.  If the move is invalid, they return false.  */
  static bool ApplyPositionCommitment (const proto::PositionCommitmentMove& mv,
                                       Phase phase,
                                       proto::BoardState& newState);
  static bool ApplySeedReveal (const proto::SeedRevealMove& mv, Phase phase,
                               proto::BoardState& newState);
  static bool ApplyShot (const proto::ShotMove& mv, Phase phase,
                         proto::BoardState& newState);
  static bool ApplyReply (const proto::ReplyMove& mv, Phase phase,
                          proto::BoardState& newState);
  static bool ApplyPositionReveal (const proto::PositionRevealMove& mv,
                                   Phase phase,
                                   proto::BoardState& newState);

  /* ShipsChannel handles the automoves and is thus directly tied to the
     board state itself.  We split the logic out nevertheless, because then
     there is a clear separation between "core board state" stuff (e.g.
     applying moves) and the logic needed only for channel daemons.  */
  friend class ShipsChannel;

  friend class BoardTests;

protected:

  bool ApplyMoveProto (const proto::BoardMove& mv,
                       proto::BoardState& newState) const override;

public:

  using BaseProtoBoardState::BaseProtoBoardState;

  bool IsValid () const override;
  int WhoseTurn () const override;
  unsigned TurnCount () const override;

  /**
   * The JSON format adds the current phase explicitly as another field
   * (in addition to the super-class-provided base64 proto).  This allows
   * frontends to make use of our GetPhase implementation more easily.
   */
  Json::Value ToJson () const override;

};

/**
 * The BoardRules instance we use for the ships game.
 */
class ShipsBoardRules : public xaya::ProtoBoardRules<ShipsBoardState>
{

public:

  xaya::ChannelProtoVersion GetProtoVersion (
      const xaya::proto::ChannelMetadata& meta) const override;

};

/**
 * Returns the initial board state of a game (i.e. just after the second
 * participant has joined).
 */
proto::BoardState InitialBoardState ();

} // namespace ships

#endif // XAYASHIPS_BOARD_HPP
