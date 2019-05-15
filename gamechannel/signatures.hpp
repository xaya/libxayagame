// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_SIGNATURES_HPP
#define GAMECHANNEL_SIGNATURES_HPP

#include "proto/metadata.pb.h"
#include "proto/signatures.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayautil/uint256.hpp>

#include <set>
#include <string>

namespace xaya
{

/**
 * Constructs the message (as string) that will be passed to "signmessage"
 * for the given channel, topic and raw data to sign.
 *
 * The topic string describes what the data is, so that e.g. a signed state
 * cannot be mistaken as a signed message stating the winner.  This string
 * must not contain any nul bytes.  "state" and "move" are reserved for use
 * with a game-specific BoardState and BoardMove value, respectively.  Other
 * values can be used for game-specific needs.
 */
std::string GetChannelSignatureMessage (const uint256& channelId,
                                        const std::string& topic,
                                        const std::string& data);

/**
 * Verifies the signatures on a SignedData instance in relation to the
 * participants and their signing keys of the given channel metadata.
 * This function returns a set of the participant indices for which a valid
 * signature was found on the data.
 *
 * The topic string describes what the data is, so that e.g. a signed state
 * cannot be mistaken as a signed message stating the winner.  This string
 * must not contain any nul bytes.  "state" and "move" are reserved for use
 * with a game-specific BoardState and BoardMove value, respectively.  Other
 * values can be used for game-specific needs.
 */
std::set<int> VerifyParticipantSignatures (XayaRpcClient& rpc,
                                           const uint256& channelId,
                                           const proto::ChannelMetadata& meta,
                                           const std::string& topic,
                                           const proto::SignedData& data);

} // namespace xaya

#endif // GAMECHANNEL_SIGNATURES_HPP
