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
                            const uint256& channelId,
                            const proto::ChannelMetadata& meta,
                            const ParsedBoardState& oldState,
                            const proto::StateTransition& transition,
                            std::set<int>& signatures,
                            std::unique_ptr<ParsedBoardState>& parsedNew)
{

  const int turn = oldState.WhoseTurn ();
  if (turn == ParsedBoardState::NO_TURN)
    {
      LOG (WARNING) << "State transition applied to 'no turn' state";
      return false;
    }

  BoardState newState;
  if (!oldState.ApplyMove (rpc, transition.move (), newState))
    {
      LOG (WARNING) << "Failed to apply move of state transition";
      return false;
    }

  parsedNew = rules.ParseState (channelId, meta, newState);
  /* newState is not user-provided but the output of a successful ApplyMove,
     so it should be guaranteed to be valid.  */
  CHECK (parsedNew != nullptr);

  if (!parsedNew->Equals (transition.new_state ().data ()))
    {
      LOG (WARNING) << "Wrong new state claimed in state transition";
      return false;
    }

  signatures = VerifyParticipantSignatures (rpc, channelId, meta, "state",
                                            transition.new_state ());
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
                       const uint256& channelId,
                       const proto::ChannelMetadata& meta,
                       const BoardState& oldState,
                       const proto::StateTransition& transition)
{
  const auto parsedOld = rules.ParseState (channelId, meta, oldState);
  if (parsedOld == nullptr)
    {
      LOG (WARNING) << "Invalid old state in state transition";
      return false;
    }

  std::unique_ptr<ParsedBoardState> parsedNew;
  std::set<int> signatures;
  return ExtraVerifyStateTransition (rpc, rules, channelId, meta, *parsedOld,
                                     transition, signatures, parsedNew);
}

bool
VerifyStateProof (XayaRpcClient& rpc, const BoardRules& rules,
                  const uint256& channelId,
                  const proto::ChannelMetadata& meta,
                  const BoardState& reinitState,
                  const proto::StateProof& proof,
                  BoardState& endState)
{
  std::set<int> signatures
      = VerifyParticipantSignatures (rpc, channelId, meta, "state",
                                     proof.initial_state ());

  auto parsed = rules.ParseState (channelId, meta,
                                  proof.initial_state ().data ());
  if (parsed == nullptr)
    {
      LOG (WARNING) << "Invalid initial state for state proof";
      return false;
    }

  endState = proof.initial_state ().data ();
  const bool foundOnChain = parsed->Equals (reinitState);

  for (const auto& t : proof.transitions ())
    {
      std::unique_ptr<ParsedBoardState> parsedNew;
      std::set<int> newSignatures;
      if (!ExtraVerifyStateTransition (rpc, rules, channelId, meta, *parsed, t,
                                       newSignatures, parsedNew))
        return false;

      signatures.insert (newSignatures.begin (), newSignatures.end ());
      parsed = std::move (parsedNew);
      endState = t.new_state ().data ();
    }

  if (foundOnChain)
    {
      VLOG (1) << "StateProof starts from reinit state and is valid";
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

const BoardState&
UnverifiedProofEndState (const proto::StateProof& proof)
{
  const int n = proof.transitions_size ();
  if (n == 0)
    return proof.initial_state ().data ();
  return proof.transitions (n - 1).new_state ().data ();
}

} // namespace xaya
