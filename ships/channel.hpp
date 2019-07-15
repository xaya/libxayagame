// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_CHANNEL_HPP
#define XAYASHIPS_CHANNEL_HPP

#include <gamechannel/boardrules.hpp>
#include <gamechannel/openchannel.hpp>
#include <gamechannel/movesender.hpp>
#include <gamechannel/proto/stateproof.pb.h>
#include <xayautil/uint256.hpp>

#include <json/json.h>

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

  /** The player name who is running this channel daemon.  */
  const std::string playerName;

public:

  ShipsChannel (const std::string& nm)
    : playerName(nm)
  {}

  ShipsChannel (const ShipsChannel&) = delete;
  void operator= (const ShipsChannel&) = delete;

  Json::Value ResolutionMove (const xaya::uint256& channelId,
                              const xaya::proto::StateProof& p) const override;
  Json::Value DisputeMove (const xaya::uint256& channelId,
                           const xaya::proto::StateProof& p) const override;

  void MaybeOnChainMove (const xaya::ParsedBoardState& state,
                         xaya::MoveSender& sender) override;

};

} // namespace ships

#endif // XAYASHIPS_CHANNEL_HPP
