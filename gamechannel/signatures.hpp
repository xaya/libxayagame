// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_SIGNATURES_HPP
#define GAMECHANNEL_SIGNATURES_HPP

#include "proto/metadata.pb.h"
#include "proto/signatures.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>

#include <set>

namespace xaya
{

/**
 * Verifies the signatures on a SignedData instance in relation to the
 * participants and their signing keys of the given channel metadata.
 * This function returns a set of the participant indices for which a valid
 * signature was found on the data.
 */
std::set<int> VerifyParticipantSignatures (XayaRpcClient& rpc,
                                           const proto::ChannelMetadata& meta,
                                           const proto::SignedData& data);

} // namespace xaya

#endif // GAMECHANNEL_SIGNATURES_HPP
