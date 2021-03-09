// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rollingstate.hpp"

#include "protoversion.hpp"
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

  /* The parsed state contains a reference to ChannelMetadata.  It should
     be exactly the one stored in the reinit entry, because otherwise we
     run the risk of having a bad reference there.  This may catch bugs
     in the code that sets latestState, and ensures there is nothing really
     strange going on.  */
  CHECK_EQ (&mit->second.latestState->GetMetadata (), mit->second.meta.get ());

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

unsigned
RollingState::GetOnChainTurnCount () const
{
  CHECK (!reinits.empty ()) << "RollingState has not been initialised yet";
  const auto mit = reinits.find (reinitId);
  CHECK (mit != reinits.end ());
  return mit->second.onChainTurn;
}

const std::string&
RollingState::GetReinitId () const
{
  CHECK (!reinits.empty ()) << "RollingState has not been initialised yet";
  return reinitId;
}

const proto::ChannelMetadata&
RollingState::GetMetadata () const
{
  CHECK (!reinits.empty ()) << "RollingState has not been initialised yet";
  const auto mit = reinits.find (reinitId);
  CHECK (mit != reinits.end ());
  return *mit->second.meta;
}

bool
RollingState::UpdateOnChain (const proto::ChannelMetadata& meta,
                             const BoardState& reinitState,
                             const proto::StateProof& proof)
{
  /* Since this comes from on-chain, it "should" be valid!  */
  CHECK (CheckVersionedProto (rules, meta, proof));

  BoardState provenState;
  CHECK (VerifyStateProof (rpc, rules, channelId, meta,
                           reinitState, proof, provenState))
      << "State proof provided on-chain is not valid";

  /* First of all, store the current on-chain update's reinit ID as the
     "latest known".  We also keep track of whether or not the ID changed,
     which we need to determine what to return for the case that the
     ID exists already and our state is not as fresh as the known one.  */
  const bool reinitChange = (reinitId != meta.reinit ());
  reinitId = meta.reinit ();
  LOG (INFO)
      << "Performing on-chain update for channel " << channelId.ToHex ()
      << " and reinitialisation " << EncodeBase64 (reinitId);

  /* Add a new entry for the reinit map if we don't have the ID yet.  */
  const auto mit = reinits.find (reinitId);
  if (mit == reinits.end ())
    {
      ReinitData entry;
      entry.meta = std::make_unique<proto::ChannelMetadata> (meta);
      entry.reinitState = reinitState;
      entry.proof = proof;
      entry.latestState = rules.ParseState (channelId, *entry.meta,
                                            provenState);
      CHECK (entry.latestState != nullptr);
      entry.onChainTurn = entry.latestState->TurnCount ();

      LOG (INFO)
          << "Added previously unknown reinitialisation.  Turn count: "
          << entry.latestState->TurnCount ();

      reinits.emplace (reinitId, std::move (entry));
      return true;
    }

  /* Update the entry to the new state, in case it is actually fresher
     than the state that we already have.  */
  ReinitData& entry = mit->second;
  CHECK (MessageDifferencer::Equals (meta, *entry.meta));
  CHECK_EQ (reinitState, entry.reinitState);

  auto parsed = rules.ParseState (channelId, *entry.meta, provenState);
  CHECK (parsed != nullptr);
  const unsigned parsedCnt = parsed->TurnCount ();
  LOG (INFO) << "Turn count provided in the update: " << parsedCnt;

  if (parsedCnt > entry.onChainTurn)
    {
      LOG (INFO) << "Updating on-chain turn count to " << parsedCnt;
      entry.onChainTurn = parsedCnt;
    }

  const unsigned currentCnt = entry.latestState->TurnCount ();
  if (currentCnt >= parsedCnt)
    {
      LOG (INFO)
          << "The new state is not fresher than the known one"
          << " with turn count " << currentCnt;
      return reinitChange;
    }

  LOG (INFO) << "The new state is fresher, updating";
  entry.proof = proof;
  entry.latestState = std::move (parsed);
  return true;
}

bool
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
      return false;
    }
  ReinitData& entry = mit->second;

  /* Verify that the StateProof proto is valid with the expected version
     and has no unknown fields.  We do not want to accept a current state
     proof that would then be invalid when put on chain!  */
  if (!CheckVersionedProto (rules, *entry.meta, proof))
    {
      LOG (WARNING) << "Off-chain update has invalid versioned state proof";
      return false;
    }

  /* Make sure that the state proof is actually valid.  In contrast to
     on-chain updates (which are filtered through the GSP), the data we get
     here comes straight from the other players and may be complete garbage.  */
  BoardState provenState;
  if (!VerifyStateProof (rpc, rules, channelId, *entry.meta,
                         entry.reinitState, proof, provenState))
    {
      LOG (WARNING)
          << "Off-chain update for channel " << channelId.ToHex ()
          << " has an invalid state proof";
      return false;
    }
  auto parsed = rules.ParseState (channelId, *entry.meta, provenState);
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
      return false;
    }

  LOG (INFO) << "The new state is fresher, updating";
  entry.proof = proof;
  entry.latestState = std::move (parsed);

  /* In this case, we return a change if and only if the update was done to
     the current reinit ID.  We do not want to signal a change when another
     reinit ID was updated (that will instead be signalled by UpdateOnChain
     when switching to that reinit ID).  */
  return (updReinit == reinitId);
}

} // namespace xaya
