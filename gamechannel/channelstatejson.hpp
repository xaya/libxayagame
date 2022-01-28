// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELSTATEJSON_HPP
#define GAMECHANNEL_CHANNELSTATEJSON_HPP

#include "boardrules.hpp"
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

} // namespace xaya

#endif // GAMECHANNEL_CHANNELSTATEJSON_HPP
