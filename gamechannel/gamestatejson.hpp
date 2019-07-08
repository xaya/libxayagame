// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_GAMESTATEJSON_HPP
#define GAMECHANNEL_GAMESTATEJSON_HPP

#include "boardrules.hpp"
#include "database.hpp"
#include "proto/metadata.pb.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>

namespace xaya
{

/**
 * Encodes a metadata proto into JSON.
 */
Json::Value ChannelMetadataToJson (const proto::ChannelMetadata& meta);

/**
 * Encodes a given state as JSON.
 */
Json::Value BoardStateToJson (const BoardRules& r, const uint256& channelId,
                              const proto::ChannelMetadata& meta,
                              const BoardState& state);

/**
 * Converts the game-state data for a given channel into JSON format.
 */
Json::Value ChannelToGameStateJson (const ChannelData& ch, const BoardRules& r);

/**
 * Returns a JSON object that represents the data for all open channels
 * in the game state.
 */
Json::Value AllChannelsGameStateJson (ChannelsTable& tbl,
                                      const BoardRules& r);

} // namespace xaya

#endif // GAMECHANNEL_GAMESTATEJSON_HPP
