// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stateproof.hpp"

#include "signatures.hpp"

#include <glog/logging.h>

#include <set>

namespace xaya
{

namespace
{

/**
 * Internal version of VerifyStateTransition, which also returns all valid
 * signatures made on the new state.  This is useful when validating a state
 * proof, so that we do not need to duplicate that check.
 */
bool
ExtraVerifyStateTransition (XayaRpcClient& rpc, const BoardRules& rules,
                            const proto::ChannelMetadata& meta,
                            const BoardState& oldState,
                            const proto::StateTransition& transition,
                            std::set<int>& signatures)
{
  const int turn = rules.WhoseTurn (meta, oldState);
  if (turn == BoardRules::NO_TURN)
    {
      LOG (WARNING) << "State transition applied to 'no turn' state";
      return false;
    }

  BoardState newState;
  if (!rules.ApplyMove (meta, oldState, transition.move (), newState))
    {
      LOG (WARNING) << "Failed to apply move of state transition";
      return false;
    }

  if (!rules.CompareStates (meta, transition.new_state ().data (), newState))
    {
      LOG (WARNING) << "Wrong new state claimed in state transition";
      return false;
    }

  signatures = VerifyParticipantSignatures (rpc, meta, transition.new_state ());
  if (signatures.count (turn) == 0)
    {
      LOG (WARNING)
          << "No valid signature of player " << turn << " on state transition";
      return false;
    }

  return true;
}

} // anonymous namespace

bool
VerifyStateTransition (XayaRpcClient& rpc, const BoardRules& rules,
                       const proto::ChannelMetadata& meta,
                       const BoardState& oldState,
                       const proto::StateTransition& transition)
{
  std::set<int> signatures;
  return ExtraVerifyStateTransition (rpc, rules, meta, oldState, transition,
                                     signatures);
}

bool
VerifyStateProof (XayaRpcClient& rpc, const BoardRules& rules,
                  const proto::ChannelMetadata& meta,
                  const BoardState& onChainState,
                  const proto::StateProof& proof,
                  BoardState& endState)
{
  std::set<int> signatures
      = VerifyParticipantSignatures (rpc, meta, proof.initial_state ());
  endState = proof.initial_state ().data ();
  bool foundOnChain = rules.CompareStates (meta, onChainState, endState);

  for (const auto& t : proof.transitions ())
    {
      std::set<int> newSignatures;
      if (!ExtraVerifyStateTransition (rpc, rules, meta, endState, t,
                                       newSignatures))
        return false;

      signatures.insert (newSignatures.begin (), newSignatures.end ());
      endState = t.new_state ().data ();
      if (!foundOnChain)
        foundOnChain = rules.CompareStates (meta, onChainState, endState);
    }

  if (foundOnChain)
    {
      VLOG (1) << "StateProof has on-chain state and is valid";
      return true;
    }

  for (int i = 0; i < meta.participants_size (); ++i)
    if (signatures.count (i) == 0)
      {
        LOG (WARNING) << "StateProof has no signature of player " << i;
        return false;
      }

  VLOG (1) << "StateProof has signatures by all players and is valid";
  return true;
}

} // namespace xaya
