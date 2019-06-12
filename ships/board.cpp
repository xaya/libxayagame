// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "board.hpp"

#include <xayautil/uint256.hpp>

#include <glog/logging.h>

namespace ships
{

bool
ShipsBoardState::IsValid () const
{
  CHECK_EQ (GetMetadata ().participants_size (), 2);

  /* If the phase is not well-defined, then the state is invalid.  */
  const auto phase = GetPhase ();
  if (phase == Phase::INVALID)
    return false;

  /* Unless the game is finished, we should have a turn set.  */
  const auto& pb = GetState ();
  if (!pb.has_turn () || phase == Phase::FINISHED)
    return !pb.has_turn () && phase == Phase::FINISHED;

  /* Since we have two players, turn should be zero or one.  */
  const int turn = pb.turn ();
  if (turn < 0 || turn > 1)
    return false;

  /* Verify some phase-dependent rules.  Especially check that turn is set
     to the correct values for phases where the turn is redundant.  */
  switch (phase)
    {
    case Phase::FIRST_COMMITMENT:
    case Phase::FIRST_REVEAL_SEED:
      if (turn != 0)
        return false;
      break;

    case Phase::SECOND_COMMITMENT:
      if (turn != 1)
        return false;
      break;

    case Phase::SHOOT:
    case Phase::ANSWER:
      /* It can be any player's turn in this case.  This is when we really
         need the turn field and it is not redundant.  */
      break;

    case Phase::SECOND_REVEAL_POSITION:
      {
        CHECK_EQ (pb.positions_size (), 2);
        const int otherTurn = 1 - turn;
        if (pb.positions (turn) != 0 || pb.positions (otherTurn) == 0)
          return false;
        break;
      }

    case Phase::WINNER_DETERMINED:
      if (turn == static_cast<int> (pb.winner ()))
        return false;
      break;

    default:
      LOG (FATAL) << "Unexpected phase: " << static_cast<int> (phase);
      return false;
    }

  return true;
}

ShipsBoardState::Phase
ShipsBoardState::GetPhase () const
{
  const auto& pb = GetState ();

  if (pb.has_winner_statement ())
    return Phase::FINISHED;
  if (pb.has_winner ())
    return Phase::WINNER_DETERMINED;

  switch (pb.position_hashes_size ())
    {
    case 0:
      return Phase::FIRST_COMMITMENT;
    case 1:
      return Phase::SECOND_COMMITMENT;
    case 2:
      break;
    default:
      return Phase::INVALID;
    }

  switch (pb.known_ships_size ())
    {
    case 0:
      return Phase::FIRST_REVEAL_SEED;
    case 2:
      break;
    default:
      return Phase::INVALID;
    }

  switch (pb.positions_size ())
    {
    case 0:
      break;
    case 2:
      return Phase::SECOND_REVEAL_POSITION;
    default:
      return Phase::INVALID;
    }

  if (pb.has_current_shot ())
    return Phase::ANSWER;
  return Phase::SHOOT;
}

int
ShipsBoardState::WhoseTurn () const
{
  if (!GetState ().has_turn ())
    return xaya::ParsedBoardState::NO_TURN;

  const int res = GetState ().turn ();
  CHECK_GE (res, 0);
  CHECK_LE (res, 1);

  return res;
}

unsigned
ShipsBoardState::TurnCount () const
{
  LOG (FATAL) << "Not implemented";
}

bool
ShipsBoardState::ApplyPositionCommitment (
    const proto::PositionCommitmentMove& mv, const Phase phase,
    proto::BoardState& newState)
{
  if (mv.position_hash ().size () != xaya::uint256::NUM_BYTES)
    {
      LOG (WARNING) << "position_hash has wrong size";
      return false;
    }

  switch (phase)
    {
    case Phase::FIRST_COMMITMENT:
      if (mv.seed_hash ().size () != xaya::uint256::NUM_BYTES)
        {
          LOG (WARNING) << "seed_hash has wrong size";
          return false;
        }
      if (mv.has_seed ())
        {
          LOG (WARNING) << "First commitment has preimage seed";
          return false;
        }

      newState.set_turn (1);
      newState.add_position_hashes (mv.position_hash ());
      CHECK_EQ (newState.position_hashes_size (), 1);
      newState.set_seed_hash_0 (mv.seed_hash ());
      return true;

    case Phase::SECOND_COMMITMENT:
      if (mv.has_seed_hash ())
        {
          LOG (WARNING) << "Second commitment has seed hash";
          return false;
        }
      if (mv.seed ().size () > xaya::uint256::NUM_BYTES)
        {
          LOG (WARNING) << "seed is too large: " << mv.seed ().size ();
          return false;
        }

      newState.set_turn (0);
      newState.add_position_hashes (mv.position_hash ());
      CHECK_EQ (newState.position_hashes_size (), 2);
      newState.set_seed_1 (mv.seed ());
      return true;

    default:
      LOG (WARNING)
          << "Invalid phase for position commitment: "
          << static_cast<int> (phase);
      return false;
    }
}

bool
ShipsBoardState::ApplyMoveProto (XayaRpcClient& rpc, const proto::BoardMove& mv,
                                 proto::BoardState& newState) const
{
  /* Moves do typically incremental changes, so we start by copying the
     current state and then modify it (rather than constructing the new
     state from scratch).  */
  const auto& pb = GetState ();
  newState = pb;

  const int turn = WhoseTurn ();
  CHECK_NE (turn, xaya::ParsedBoardState::NO_TURN);

  const auto phase = GetPhase ();
  switch (mv.move_case ())
    {
    case proto::BoardMove::kPositionCommitment:
      return ApplyPositionCommitment (mv.position_commitment (), phase,
                                      newState);

    default:
    case proto::BoardMove::MOVE_NOT_SET:
      LOG (WARNING) << "Move does not specify any one-of case";
      return false;
    }

  LOG (FATAL) << "Unexpected move case: " << mv.move_case ();
  return false;
}

} // namespace ships
