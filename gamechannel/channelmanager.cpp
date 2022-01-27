// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelmanager.hpp"

#include "gamestatejson.hpp"
#include "stateproof.hpp"

namespace xaya
{

ChannelManager::DisputeData::DisputeData ()
{
  pendingResolution.SetNull ();
}

ChannelManager::ChannelManager (const BoardRules& r, OpenChannel& oc,
                                const SignatureVerifier& v, SignatureSigner& s,
                                const uint256& id, const std::string& name)
  : rules(r), game(oc), verifier(v), signer(s), channelId(id), playerName(name),
    boardStates(rules, verifier, channelId)
{
  blockHash.SetNull ();
  pendingPutStateOnChain.SetNull ();
  pendingDispute.SetNull ();
}

void
ChannelManager::SetOffChainBroadcast (OffChainBroadcast& s)
{
  CHECK (offChainSender == nullptr);
  offChainSender = &s;
}

void
ChannelManager::SetMoveSender (MoveSender& s)
{
  CHECK (onChainSender == nullptr);
  onChainSender = &s;
}

void
ChannelManager::TryResolveDispute ()
{
  VLOG (1)
      << "Trying to resolve a potential dispute for channel "
      << channelId.ToHex ();

  if (!exists)
    {
      VLOG (1) << "This channel does not exist on-chain";
      return;
    }
  if (dispute == nullptr)
    {
      VLOG (1) << "There is no dispute for the channel";
      return;
    }
  if (!dispute->pendingResolution.IsNull ())
    {
      VLOG (1) << "There may be a pending resolution already";
      return;
    }

  CHECK_NE (dispute->turn, ParsedBoardState::NO_TURN);
  const auto& meta = boardStates.GetMetadata ();
  CHECK_GE (dispute->turn, 0);
  CHECK_LT (dispute->turn, meta.participants_size ());
  const std::string& disputedPlayer = meta.participants (dispute->turn).name ();
  if (disputedPlayer != playerName)
    {
      VLOG (1)
          << "Disputed player is " << disputedPlayer
          << ", we are " << playerName;
      return;
    }

  const unsigned latestCnt = boardStates.GetLatestState ().TurnCount ();
  if (latestCnt <= dispute->count)
    {
      VLOG (1)
          << "We have no better state than the disputed turn count "
          << dispute->count;
      return;
    }

  LOG (INFO)
      << "Channel " << channelId.ToHex ()
      << " has a dispute for our turn, we have a better state"
      << " at turn count " << latestCnt
      << " (dispute: " << dispute->count << ")";
  CHECK (onChainSender != nullptr);
  dispute->pendingResolution
      = onChainSender->SendResolution (boardStates.GetStateProof ());
}

bool
ChannelManager::ProcessAutoMoves ()
{
  VLOG (1) << "Processing potential auto moves...";
  bool found = false;
  while (true)
    {
      const auto& state = boardStates.GetLatestState ();

      const auto& meta = boardStates.GetMetadata ();
      const int turn = state.WhoseTurn ();
      if (turn == ParsedBoardState::NO_TURN)
        {
          VLOG (1) << "We are in a no-turn state";
          break;
        }
      CHECK_GE (turn, 0);
      CHECK_LT (turn, meta.participants_size ());
      if (meta.participants (turn).name () != playerName)
        {
          VLOG (1) << "It is not our turn";
          break;
        }

      BoardMove mv;
      if (!game.MaybeAutoMove (state, mv))
        {
          VLOG (1) << "I didn't find an automove";
          break;
        }

      LOG (INFO) << "Found automove: " << mv;
      CHECK (ApplyLocalMove (mv));
      found = true;
    }

  return found;
}

void
ChannelManager::ProcessStateUpdate (bool broadcast)
{
  if (ProcessAutoMoves ())
    broadcast = true;

  if (broadcast)
    {
      CHECK (offChainSender != nullptr);
      offChainSender->SendNewState (boardStates.GetReinitId (),
                                    boardStates.GetStateProof ());
    }

  TryResolveDispute ();

  if (onChainSender != nullptr)
    game.MaybeOnChainMove (boardStates.GetLatestState (), *onChainSender);

  NotifyStateChange ();
}

void
ChannelManager::ProcessOffChain (const std::string& reinitId,
                                 const proto::StateProof& proof)
{
  if (!boardStates.UpdateWithMove (reinitId, proof))
    return;

  ProcessStateUpdate (false);
}

void
ChannelManager::ProcessOnChainNonExistant (const uint256& blk, const unsigned h)
{
  LOG_IF (INFO, exists)
      << "Channel " << channelId.ToHex () << " no longer exists on-chain";

  blockHash = blk;
  onChainHeight = h;

  exists = false;

  /* If the channel no longer exists on chain, set the list of participants
     for the broadcaster to empty.  */
  if (offChainSender != nullptr)
    offChainSender->SetParticipants (proto::ChannelMetadata ());

  NotifyStateChange ();
}

namespace
{

/**
 * If a txid is non-null, check if it is pending.  If it is not,
 * reset it to null.  This is the common logic we apply for disputes and
 * resolutions when a new block comes in.
 */
void
ResetMinedTxid (MoveSender* sender, uint256& txid)
{
  if (txid.IsNull ())
    return;

  /* Somehow we must have sent the previous tx!  */
  CHECK (sender != nullptr);

  if (sender->IsPending (txid))
    {
      LOG (INFO) << "Transaction " << txid.ToHex () << " is still pending";
      return;
    }

  LOG (INFO) << "Transaction " << txid.ToHex () << " is no longer pending";
  txid.SetNull ();
}

} // anonymous namespace

void
ChannelManager::ProcessOnChain (const uint256& blk, const unsigned h,
                                const proto::ChannelMetadata& meta,
                                const BoardState& reinitState,
                                const proto::StateProof& proof,
                                const unsigned disputeHeight)
{
  LOG_IF (INFO, !exists)
      << "Channel " << channelId.ToHex () << " is now found on-chain";

  blockHash = blk;
  onChainHeight = h;

  ResetMinedTxid (onChainSender, pendingPutStateOnChain);
  ResetMinedTxid (onChainSender, pendingDispute);
  exists = true;
  boardStates.UpdateOnChain (meta, reinitState, proof);

  if (disputeHeight == 0)
    {
      LOG_IF (INFO, dispute != nullptr)
          << "Dispute on channel " << channelId.ToHex () << " is resolved";
      dispute.reset ();
    }
  else
    {
      if (dispute == nullptr)
        {
          LOG (INFO)
              << "Channel " << channelId.ToHex ()
              << " has now a dispute for height " << disputeHeight;
          dispute = std::make_unique<DisputeData> ();
        }

      dispute->height = disputeHeight;
      ResetMinedTxid (onChainSender, dispute->pendingResolution);

      auto p = rules.ParseState (channelId, meta,
                                 UnverifiedProofEndState (proof));
      dispute->turn = p->WhoseTurn ();
      dispute->count = p->TurnCount ();
    }

  /* Update the list of participants for the off-chain broadcaster to the
     latest known version.  */
  if (offChainSender != nullptr)
    offChainSender->SetParticipants (meta);

  ProcessStateUpdate (false);
}

bool
ChannelManager::ApplyLocalMove (const BoardMove& mv)
{
  CHECK (exists);

  proto::StateProof newProof;
  if (!ExtendStateProof (verifier, signer, rules, channelId,
                         boardStates.GetMetadata (),
                         boardStates.GetStateProof (), mv, newProof))
    {
      LOG (ERROR) << "Failed to extend state with local move";
      return false;
    }

  /* The update is guaranteed to yield a change at this point, since otherwise
     ExtendStateProof would already have failed.  */
  CHECK (boardStates.UpdateWithMove (boardStates.GetReinitId (), newProof));

  return true;
}

void
ChannelManager::ProcessLocalMove (const BoardMove& mv)
{
  LOG (INFO) << "Local move: " << mv;

  if (!exists)
    {
      LOG (ERROR) << "Channel does not exist on chain, ingoring local move";
      return;
    }

  if (!ApplyLocalMove (mv))
    return;

  ProcessStateUpdate (true);
}

void
ChannelManager::TriggerAutoMoves ()
{
  if (!exists)
    {
      LOG (INFO) << "Channel does not exist on chain, not triggering automoves";
      return;
    }

  if (!ProcessAutoMoves ())
    {
      LOG (INFO) << "Automoves triggered explicitly, but none found";
      return;
    }

  ProcessStateUpdate (true);
}

uint256
ChannelManager::PutStateOnChain ()
{
  LOG (INFO)
      << "Trying to put the latest state on chain for " << channelId.ToHex ();

  uint256 txidNull;
  txidNull.SetNull ();

  if (!exists)
    {
      LOG (WARNING) << "The channel does not exist on chain";
      return txidNull;
    }

  const unsigned latestCnt = boardStates.GetLatestState ().TurnCount ();
  const unsigned onChainCnt = boardStates.GetOnChainTurnCount ();
  if (latestCnt <= boardStates.GetOnChainTurnCount ())
    {
      /* We always update the latest state based on what we get on chain,
         so it should not happen that the on-chain count is actually
         better than the latest state.  */
      CHECK_EQ (latestCnt, onChainCnt);
      LOG (WARNING)
          << "Latest state on chain matches the best known state already"
          << " at turn count " << onChainCnt
          << ", not sending the state on chain";
      return txidNull;
    }

  CHECK (onChainSender != nullptr);
  pendingPutStateOnChain
      = onChainSender->SendResolution (boardStates.GetStateProof ());
  return pendingPutStateOnChain;
}

uint256
ChannelManager::FileDispute ()
{
  LOG (INFO) << "Trying to file a dispute for channel " << channelId.ToHex ();

  uint256 txidNull;
  txidNull.SetNull ();

  if (!exists)
    {
      LOG (WARNING) << "The channel does not exist on chain";
      return txidNull;
    }
  if (dispute != nullptr)
    {
      LOG (WARNING) << "There is already a dispute for the channel";
      return txidNull;
    }
  if (!pendingDispute.IsNull ())
    {
      LOG (WARNING) << "There may already be a pending dispute";
      return txidNull;
    }

  CHECK (onChainSender != nullptr);
  pendingDispute = onChainSender->SendDispute (boardStates.GetStateProof ());
  return pendingDispute;
}

Json::Value
ChannelManager::ToJson () const
{
  Json::Value res(Json::objectValue);
  res["id"] = channelId.ToHex ();
  res["playername"] = playerName;
  res["existsonchain"] = exists;
  res["version"] = stateVersion;

  if (!blockHash.IsNull ())
    {
      res["blockhash"] = blockHash.ToHex ();
      res["height"] = static_cast<int> (onChainHeight);
    }

  if (!exists)
    return res;

  Json::Value current(Json::objectValue);
  const auto& meta = boardStates.GetMetadata ();
  const auto& proof = boardStates.GetStateProof ();
  current["meta"] = ChannelMetadataToJson (boardStates.GetMetadata ());
  current["state"] = BoardStateToJson (rules, channelId, meta,
                                       UnverifiedProofEndState (proof));
  res["current"] = current;

  if (dispute != nullptr)
    {
      Json::Value disp(Json::objectValue);
      disp["height"] = static_cast<int> (dispute->height);
      disp["whoseturn"] = dispute->turn;

      const unsigned knownCount = boardStates.GetLatestState ().TurnCount ();
      disp["canresolve"] = (knownCount > dispute->count);

      res["dispute"] = disp;
    }

  Json::Value pending(Json::objectValue);
  if (!pendingPutStateOnChain.IsNull ())
    pending["putstateonchain"] = pendingPutStateOnChain.ToHex ();
  if (!pendingDispute.IsNull ())
    pending["dispute"] = pendingDispute.ToHex ();
  if (dispute != nullptr && !dispute->pendingResolution.IsNull ())
    pending["resolution"] = dispute->pendingResolution.ToHex ();
  res["pending"] = pending;

  return res;
}

void
ChannelManager::NotifyStateChange ()
{
  /* Callers are expected to hold a lock on mut already, as is the case in
     all the updating code that will call here anyway.  */
  CHECK_GT (stateVersion, 0);
  ++stateVersion;
  VLOG (1)
      << "Notifying about state change, new version: "
      << stateVersion;
  for (auto* cb : callbacks)
    cb->StateChanged ();
}

void
ChannelManager::RegisterCallback (Callbacks& cb)
{
  callbacks.insert (&cb);
}

void
ChannelManager::UnregisterCallback (Callbacks& cb)
{
  callbacks.erase (&cb);
}

} // namespace xaya
