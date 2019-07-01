// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_STATEPROOF_HPP
#define GAMECHANNEL_STATEPROOF_HPP

#include "boardrules.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayautil/uint256.hpp>

namespace xaya
{

/**
 * Checks if a given state transition is valid from the current state.
 * Returns true if it is.
 *
 * A state transition is valid if the move is valid from old state -> new state
 * and the player who was supposed to make that move signed the new state.
 */
bool VerifyStateTransition (XayaRpcClient& rpc, const BoardRules& rules,
                            const uint256& channelId,
                            const proto::ChannelMetadata& meta,
                            const BoardState& oldState,
                            const proto::StateTransition& transition);

/**
 * Verifies a state proof for the given channel.  If the proof is complete
 * and valid, then true is returned and the resulting board state is returned
 * in endState.
 */
bool VerifyStateProof (XayaRpcClient& rpc, const BoardRules& rules,
                       const uint256& channelId,
                       const proto::ChannelMetadata& meta,
                       const BoardState& reinitState,
                       const proto::StateProof& proof,
                       BoardState& endState);

} // namespace xaya

#endif // GAMECHANNEL_STATEPROOF_HPP
