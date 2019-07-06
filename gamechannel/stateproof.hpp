// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_STATEPROOF_HPP
#define GAMECHANNEL_STATEPROOF_HPP

#include "boardrules.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
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

/**
 * Extracts the endstate from a StateProof without checking it.  This is useful
 * if it has been checked already or is otherwise known to be good (e.g. because
 * it was retrieved from the on-chain GSP).  In that situation, it is a lot more
 * efficient than VerifyStateProof.
 */
const BoardState& UnverifiedProofEndState (const proto::StateProof& proof);

/**
 * Tries to apply the given move onto the latest state of the given proof,
 * updating the proof for the new state if possible (signing it through
 * the given wallet connection).
 *
 * The state proof must be known to be valid already (e.g. because it is
 * the on-chain state from the GSP, or because it has been validated previously
 * already).
 *
 * Returns true if the state proof was extended successfully.
 */
bool ExtendStateProof (XayaRpcClient& rpc, XayaWalletRpcClient& wallet,
                       const BoardRules& rules,
                       const uint256& channelId,
                       const proto::ChannelMetadata& meta,
                       const proto::StateProof& oldProof,
                       const BoardMove& mv,
                       proto::StateProof& newProof);

} // namespace xaya

#endif // GAMECHANNEL_STATEPROOF_HPP
