// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stateproof.hpp"

#include "signatures.hpp"

#include <xayautil/base64.hpp>

#include <jsonrpccpp/common/exception.h>

#include <glog/logging.h>

#include <iterator>
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

bool
ExtendStateProof (XayaRpcClient& rpc, XayaWalletRpcClient& wallet,
                  const BoardRules& rules,
                  const uint256& channelId,
                  const proto::ChannelMetadata& meta,
                  const proto::StateProof& oldProof,
                  const BoardMove& mv,
                  proto::StateProof& newProof)
{
  const BoardState oldState = UnverifiedProofEndState (oldProof);
  const auto parsedOld = rules.ParseState (channelId, meta, oldState);
  CHECK (parsedOld != nullptr) << "Invalid state-proof endstate: " << oldState;

  const int turn = parsedOld->WhoseTurn ();
  if (turn == ParsedBoardState::NO_TURN)
    {
      LOG (ERROR) << "Cannot extend state proof in no-turn state";
      return false;
    }
  CHECK_GE (turn, 0);
  CHECK_LT (turn, meta.participants_size ());
  const std::string& addr = meta.participants (turn).address ();

  BoardState newState;
  if (!parsedOld->ApplyMove (rpc, mv, newState))
    {
      LOG (ERROR) << "Invalid move for extending a state proof: " << mv;
      return false;
    }

  proto::StateTransition trans;
  trans.set_move (mv);
  auto* ns = trans.mutable_new_state ();
  ns->set_data (newState);

  LOG (INFO)
      << "Trying to sign new state for participant " << turn
      << " with address " << addr;
  try
    {
      const auto& msg
          = GetChannelSignatureMessage (channelId, meta, "state", newState);
      const std::string sgn = wallet.signmessage (addr, msg);
      CHECK (DecodeBase64 (sgn, *ns->add_signatures ()));
    }
  catch (const jsonrpc::JsonRpcException& exc)
    {
      LOG (ERROR) << "Signature with " << addr << " failed: " << exc.what ();
      return false;
    }

  /* We got a valid signature of the new state.  Now we have to figure out what
     the "minimal" valid state proof for the new state is.  For this, we first
     "normalise" all state transitions (including the old initial state and
     the new last transition) into one large array, and then find the
     trailing subset of it that is sufficient.  */

  std::vector<proto::StateTransition> transitions;
  transitions.emplace_back ();
  *transitions.back ().mutable_new_state () = oldProof.initial_state ();
  for (const auto& t : oldProof.transitions ())
    transitions.push_back (t);
  transitions.emplace_back (std::move (trans));

  std::set<int> signatures;
  auto begin = std::prev (transitions.end ());
  const size_t n = meta.participants_size ();
  while (true)
    {
      const auto newSigs
          = VerifyParticipantSignatures (rpc, channelId, meta, "state",
                                         begin->new_state ());
      signatures.insert (newSigs.begin (), newSigs.end ());

      CHECK_LE (signatures.size (), n);
      if (signatures.size () == n || begin == transitions.begin ())
        break;

      --begin;
    }

  newProof.Clear ();
  for (auto it = begin; it != transitions.end (); ++it)
    {
      if (it == begin)
        newProof.mutable_initial_state ()->Swap (it->mutable_new_state ());
      else
        newProof.add_transitions ()->Swap (&*it);
    }

  return true;
}

} // namespace xaya
