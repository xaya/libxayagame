// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channel.hpp"

#include "proto/winnerstatement.pb.h"

#include <gamechannel/proto/signatures.pb.h>
#include <gamechannel/protoutils.hpp>
#include <gamechannel/signatures.hpp>
#include <xayautil/hash.hpp>

namespace ships
{

ShipsChannel::ShipsChannel (XayaWalletRpcClient& w, const std::string& nm)
  : wallet(w), playerName(nm)
{
  txidClose.SetNull ();
}

bool
ShipsChannel::IsPositionSet () const
{
  return position.GetBits () != 0;
}

const Grid&
ShipsChannel::GetPosition () const
{
  CHECK (IsPositionSet ());
  return position;
}

void
ShipsChannel::SetPosition (const Grid& g)
{
  CHECK (!IsPositionSet ());

  if (!VerifyPositionOfShips (g))
    {
      LOG (ERROR)
          << "Cannot set " << g.GetBits () << " as position, that is invalid";
      return;
    }

  position = g;
  positionSalt = rnd.Get<xaya::uint256> ();
  LOG (INFO)
      << "Stored player position " << position.GetBits ()
      << " and generated salt: " << positionSalt.ToHex ();

  CHECK (IsPositionSet ());
}

proto::BoardMove
ShipsChannel::GetShotMove (const Coord& c) const
{
  CHECK (c.IsOnBoard ());

  proto::BoardMove res;
  res.mutable_shot ()->set_location (c.GetIndex ());

  return res;
}

proto::BoardMove
ShipsChannel::GetPositionRevealMove () const
{
  CHECK (IsPositionSet ());

  proto::BoardMove res;
  auto* reveal = res.mutable_position_reveal ();
  reveal->set_position (position.GetBits ());
  reveal->set_salt (positionSalt.GetBinaryString ());

  return res;
}

int
ShipsChannel::GetPlayerIndex (const ShipsBoardState& state) const
{
  const auto& meta = state.GetMetadata ();

  int res = -1;
  for (int i = 0; i < meta.participants_size (); ++i)
    if (meta.participants (i).name () == playerName)
      {
        CHECK_EQ (res, -1);
        res = i;
      }

  CHECK_GE (res, 0);
  CHECK_LE (res, 1);
  return res;
}

bool
ShipsChannel::AutoPositionCommitment (proto::BoardMove& mv)
{
  if (!IsPositionSet ())
    return false;

  xaya::SHA256 hasher;
  hasher << position.Blob () << positionSalt;
  const std::string hashStr = hasher.Finalise ().GetBinaryString ();

  mv.mutable_position_commitment ()->set_position_hash (hashStr);
  return true;
}

bool
ShipsChannel::InternalAutoMove (const ShipsBoardState& state,
                                proto::BoardMove& mv)
{
  const auto& pb = state.GetState ();

  const int index = GetPlayerIndex (state);
  CHECK_EQ (index, pb.turn ());

  const auto phase = state.GetPhase ();
  switch (phase)
    {
    case ShipsBoardState::Phase::FIRST_COMMITMENT:
      {
        CHECK_EQ (index, 0);

        if (!AutoPositionCommitment (mv))
          return false;

        seed0 = rnd.Get<xaya::uint256> ();
        LOG (INFO) << "Random seed for first player: " << seed0.ToHex ();

        xaya::SHA256 seedHasher;
        seedHasher << seed0;
        const std::string seedHash = seedHasher.Finalise ().GetBinaryString ();

        mv.mutable_position_commitment ()->set_seed_hash (seedHash);
        return true;
      }

    case ShipsBoardState::Phase::SECOND_COMMITMENT:
      {
        CHECK_EQ (index, 1);

        if (!AutoPositionCommitment (mv))
          return false;

        const auto seed1 = rnd.Get<xaya::uint256> ();
        LOG (INFO) << "Random seed for second player: " << seed1.ToHex ();

        mv.mutable_position_commitment ()->set_seed (seed1.GetBinaryString ());
        return true;
      }

    case ShipsBoardState::Phase::FIRST_REVEAL_SEED:
      {
        CHECK_EQ (index, 0);
        mv.mutable_seed_reveal ()->set_seed (seed0.GetBinaryString ());
        return true;
      }

    case ShipsBoardState::Phase::SHOOT:
      {
        /* If we already hit all ships of the opponent, then we go on
           to reveal our position to ensure that we win.  */

        const int other = 1 - index;
        CHECK_GE (other, 0);
        CHECK_LE (other, 1);

        const Grid hits(pb.known_ships (other).hits ());
        if (hits.CountOnes () >= Grid::TotalShipCells ())
          {
            LOG (INFO) << "We hit all opponent ships, revealing";
            mv = GetPositionRevealMove ();
            return true;
          }

        return false;
      }

    case ShipsBoardState::Phase::ANSWER:
      {
        CHECK (IsPositionSet ());
        const Coord target(pb.current_shot ());
        CHECK (target.IsOnBoard ());

        const bool hit = position.Get (target);
        mv.mutable_reply ()->set_reply (hit ? proto::ReplyMove::HIT
                                            : proto::ReplyMove::MISS);
        return true;
      }

    case ShipsBoardState::Phase::SECOND_REVEAL_POSITION:
      {
        mv = GetPositionRevealMove ();
        return true;
      }

    case ShipsBoardState::Phase::WINNER_DETERMINED:
      /* We used to sign the winner statement here, but now the game
         ends unofficially at this stage (even though in theory the board rules
         still support adding a winner statement later on).  */
      return false;

    case ShipsBoardState::Phase::FINISHED:
    default:
      LOG (FATAL)
          << "Invalid phase for auto move: " << static_cast<int> (phase);
    }
}

namespace
{

Json::Value
DisputeResolutionMove (const std::string& type,
                       const xaya::uint256& channelId,
                       const xaya::proto::StateProof& p)
{
  Json::Value data(Json::objectValue);
  data["id"] = channelId.ToHex ();
  data["state"] = xaya::ProtoToBase64 (p);

  Json::Value res(Json::objectValue);
  res[type] = data;

  return res;
}

} // anonymous namespace

Json::Value
ShipsChannel::ResolutionMove (const xaya::uint256& channelId,
                              const xaya::proto::StateProof& p) const
{
  return DisputeResolutionMove ("r", channelId, p);
}

Json::Value
ShipsChannel::DisputeMove (const xaya::uint256& channelId,
                           const xaya::proto::StateProof& p) const
{
  return DisputeResolutionMove ("d", channelId, p);
}

bool
ShipsChannel::MaybeAutoMove (const xaya::ParsedBoardState& state,
                             xaya::BoardMove& mv)
{
  const auto& shipsState = dynamic_cast<const ShipsBoardState&> (state);

  proto::BoardMove mvPb;
  if (!InternalAutoMove (shipsState, mvPb))
    return false;

  CHECK (mvPb.SerializeToString (&mv));
  return true;
}

void
ShipsChannel::MaybeOnChainMove (const xaya::ParsedBoardState& state,
                                xaya::MoveSender& sender)
{
  const auto& shipsState = dynamic_cast<const ShipsBoardState&> (state);
  const auto& id = shipsState.GetChannelId ();
  const auto& meta = shipsState.GetMetadata ();

  if (shipsState.GetPhase () != ShipsBoardState::Phase::WINNER_DETERMINED)
    return;

  const auto& statePb = shipsState.GetState ();
  CHECK (statePb.has_winner ());

  const int loser = 1 - statePb.winner ();
  CHECK_GE (loser, 0);
  CHECK_LT (loser, meta.participants_size ());
  if (meta.participants (loser).name () != playerName)
    return;

  if (!txidClose.IsNull () && sender.IsPending (txidClose))
    {
      /* If we already have a pending close move, then we are not sending
         another.  Note that there is a slight chance that this has weird
         behaviour with reorgs:  Namely if the original join gets reorged
         and another second player joins, it could happen that our pending
         close is invalid (because it was signed by the previous opponent).

         But this is very unlikely to happen.  And even if it does, there
         is not much harm.  The worst that can happen is that we wait for
         the current move to be confirmed, and then send a correct new one.  */

      LOG (INFO)
          << "We already have a pending channel close: "
          << txidClose.ToHex ();
      return;
    }

  Json::Value data(Json::objectValue);
  data["id"] = id.ToHex ();

  Json::Value mv(Json::objectValue);
  mv["l"] = data;
  txidClose = sender.SendMove (mv);
  LOG (INFO)
      << "Channel has a winner and we lost, closing on-chain: "
      << txidClose.ToHex ();
}

} // namespace ships
