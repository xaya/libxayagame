// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channel.hpp"

#include "board.hpp"
#include "proto/winnerstatement.pb.h"

#include <gamechannel/proto/signatures.pb.h>
#include <gamechannel/protoutils.hpp>

namespace ships
{

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

void
ShipsChannel::MaybeOnChainMove (const xaya::ParsedBoardState& state,
                                xaya::MoveSender& sender)
{
  const auto& shipsState = dynamic_cast<const ShipsBoardState&> (state);
  const auto& id = shipsState.GetChannelId ();
  const auto& meta = shipsState.GetMetadata ();

  if (shipsState.GetPhase () != ShipsBoardState::Phase::FINISHED)
    return;

  const auto& statePb = shipsState.GetState ();
  CHECK (statePb.has_winner_statement ());
  const auto& signedStmt = statePb.winner_statement ();

  proto::WinnerStatement stmt;
  CHECK (stmt.ParseFromString (signedStmt.data ()));
  CHECK (stmt.has_winner ());
  CHECK_GE (stmt.winner (), 0);
  CHECK_LT (stmt.winner (), meta.participants_size ());
  if (meta.participants (stmt.winner ()).name () != playerName)
    return;

  LOG (INFO) << "Channel has a winner statement and we won, closing on-chain";

  Json::Value data(Json::objectValue);
  data["id"] = id.ToHex ();
  data["stmt"] = xaya::ProtoToBase64 (signedStmt);

  Json::Value mv(Json::objectValue);
  mv["w"] = data;
  sender.SendMove (mv);
}

} // namespace ships
