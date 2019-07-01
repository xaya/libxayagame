// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rollingstate.hpp"

#include "stateproof.hpp"

#include <xayautil/base64.hpp>

#include <google/protobuf/util/message_differencer.h>

#include <glog/logging.h>

using google::protobuf::util::MessageDifferencer;

namespace xaya
{

const ParsedBoardState&
RollingState::GetLatestState () const
{
  CHECK (!reinits.empty ()) << "RollingState has not been initialised yet";
  const auto mit = reinits.find (reinitId);
  CHECK (mit != reinits.end ());
  return *mit->second.latestState;
}

const proto::StateProof&
RollingState::GetStateProof () const
{
  CHECK (!reinits.empty ()) << "RollingState has not been initialised yet";
  const auto mit = reinits.find (reinitId);
  CHECK (mit != reinits.end ());
  return mit->second.proof;
}

const std::string&
RollingState::GetReinitId () const
{
  CHECK (!reinits.empty ()) << "RollingState has not been initialised yet";
  return reinitId;
}

void
RollingState::UpdateOnChain (const proto::ChannelMetadata& meta,
                             const BoardState& reinitState,
                             const proto::StateProof& proof)
{
  BoardState provenState;
  CHECK (VerifyStateProof (rpc, rules, channelId, meta,
                           reinitState, proof, provenState))
      << "State proof provided on-chain is not valid";
  auto parsed = rules.ParseState (channelId, meta, provenState);
  CHECK (parsed != nullptr);
  const unsigned parsedCnt = parsed->TurnCount ();

  /* First of all, store the current on-chain update's reinit ID as the
     "latest known".  */
  reinitId = meta.reinit ();
  LOG (INFO)
      << "Performing on-chain update for channel " << channelId.ToHex ()
      << " and reinitialisation " << EncodeBase64 (reinitId);
  LOG (INFO) << "Turn count provided in the update: " << parsedCnt;

  /* Add a new entry for the reinit map if we don't have the ID yet.  */
  const auto mit = reinits.find (reinitId);
  if (mit == reinits.end ())
    {
      ReinitData entry;
      entry.meta = meta;
      entry.reinitState = reinitState;
      entry.proof = proof;
      entry.latestState = std::move (parsed);
      CHECK (entry.latestState != nullptr);
      reinits.emplace (reinitId, std::move (entry));

      LOG (INFO) << "Added previously unknown reinitialisation";
      return;
    }

  /* Update the entry to the new state, in case it is actually fresher
     than the state that we already have.  */
  ReinitData& entry = mit->second;
  CHECK (MessageDifferencer::Equals (meta, entry.meta));
  CHECK_EQ (reinitState, entry.reinitState);

  const unsigned currentCnt = entry.latestState->TurnCount ();
  if (currentCnt >= parsedCnt)
    {
      LOG (INFO)
          << "The new state is not fresher than the known one"
          << " with turn count " << currentCnt;
      return;
    }

  LOG (INFO) << "The new state is fresher, updating";
  entry.proof = proof;
  entry.latestState = std::move (parsed);
}

void
RollingState::UpdateWithMove (const std::string& updReinit,
                              const proto::StateProof& proof)
{
  /* For this update, we do not care whether the reinit ID is the "current" one
     or not.  We simply update the associated state if have any, so that we
     stay up-to-date as much as possible.  For instance, it could happen that
     we receive an off-chain move later than an on-chain update that changed
     the channel; then we still want to apply the off-chain update, in case
     the on-chain move gets detached again or something like that.  */

  const auto mit = reinits.find (updReinit);
  if (mit == reinits.end ())
    {
      LOG (WARNING)
          << "Off-chain update for channel " << channelId.ToHex ()
          << " has unknown reinitialisation ID: " << EncodeBase64 (updReinit);
      return;
    }
  ReinitData& entry = mit->second;

  /* Make sure that the state proof is actually valid.  In contrast to
     on-chain updates (which are filtered through the GSP), the data we get
     here comes straight from the other players and may be complete garbage.  */
  BoardState provenState;
  if (!VerifyStateProof (rpc, rules, channelId, entry.meta,
                         entry.reinitState, proof, provenState))
    {
      LOG (WARNING)
          << "Off-chain update for channel " << channelId.ToHex ()
          << " has an invalid state proof";
      return;
    }
  auto parsed = rules.ParseState (channelId, entry.meta, provenState);
  CHECK (parsed != nullptr);

  /* The state proof is valid.  Update our state if the provided one is actually
     fresher than what we have already.  */
  const unsigned parsedCnt = parsed->TurnCount ();
  LOG (INFO)
      << "Received off-chain update for channel " << channelId.ToHex ()
      << " with turn count " << parsedCnt;

  const unsigned currentCnt = entry.latestState->TurnCount ();
  if (currentCnt >= parsedCnt)
    {
      LOG (INFO)
          << "The new state is not fresher than the known one"
          << " with turn count " << currentCnt;
      return;
    }

  LOG (INFO) << "The new state is fresher, updating";
  entry.proof = proof;
  entry.latestState = std::move (parsed);
}

} // namespace xaya
