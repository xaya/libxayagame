// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelmanager.hpp"

#include "gamestatejson.hpp"
#include "stateproof.hpp"

#include <glog/logging.h>

namespace xaya
{

ChannelManager::ChannelManager (const BoardRules& r,
                                XayaRpcClient& c, XayaWalletRpcClient& w,
                                const uint256& id, const std::string& name)
  : rules(r), rpc(c), wallet(w), channelId(id), playerName(name),
    boardStates(rules, rpc, channelId)
{}

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
  if (dispute->pendingResolution)
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
  onChainSender->SendResolution (boardStates.GetStateProof ());
  dispute->pendingResolution = true;
}

void
ChannelManager::ProcessOffChain (const std::string& reinitId,
                                 const proto::StateProof& proof)
{
  std::lock_guard<std::mutex> lock(mut);

  boardStates.UpdateWithMove (reinitId, proof);

  TryResolveDispute ();
  NotifyStateChange ();
}

void
ChannelManager::ProcessOnChainNonExistant ()
{
  LOG_IF (INFO, exists)
      << "Channel " << channelId.ToHex () << " no longer exists on-chain";
  std::lock_guard<std::mutex> lock(mut);

  exists = false;
  NotifyStateChange ();
}

void
ChannelManager::ProcessOnChain (const proto::ChannelMetadata& meta,
                                const BoardState& reinitState,
                                const proto::StateProof& proof,
                                const unsigned disputeHeight)
{
  LOG_IF (INFO, !exists)
      << "Channel " << channelId.ToHex () << " is now found on-chain";
  std::lock_guard<std::mutex> lock(mut);

  pendingDispute = false;
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
      dispute->pendingResolution = false;

      auto p = rules.ParseState (channelId, meta,
                                 UnverifiedProofEndState (proof));
      dispute->turn = p->WhoseTurn ();
      dispute->count = p->TurnCount ();
    }

  TryResolveDispute ();
  NotifyStateChange ();
}

void
ChannelManager::ProcessLocalMove (const BoardMove& mv)
{
  LOG (INFO) << "Local move: " << mv;
  std::lock_guard<std::mutex> lock(mut);

  if (!exists)
    {
      LOG (ERROR) << "Channel does not exist on chain, ingoring local move";
      return;
    }

  proto::StateProof newProof;
  if (!ExtendStateProof (rpc, wallet, rules, channelId,
                         boardStates.GetMetadata (),
                         boardStates.GetStateProof (), mv, newProof))
    {
      LOG (ERROR) << "Failed to extend state with local move";
      return;
    }

  const auto& reinit = boardStates.GetReinitId ();
  boardStates.UpdateWithMove (reinit, newProof);

  CHECK (offChainSender != nullptr);
  offChainSender->SendNewState (reinit, newProof);

  TryResolveDispute ();
  NotifyStateChange ();
}

void
ChannelManager::FileDispute ()
{
  LOG (INFO) << "Trying to file a dispute for channel " << channelId.ToHex ();
  std::lock_guard<std::mutex> lock(mut);

  if (!exists)
    {
      LOG (WARNING) << "The channel does not exist on chain";
      return;
    }
  if (dispute != nullptr)
    {
      LOG (WARNING) << "There is already a dispute for the channel";
      return;
    }
  if (pendingDispute)
    {
      LOG (WARNING) << "There may already be a pending dispute";
      return;
    }

  CHECK (onChainSender != nullptr);
  onChainSender->SendDispute (boardStates.GetStateProof ());
  pendingDispute = true;
}

Json::Value
ChannelManager::UnlockedToJson () const
{
  Json::Value res(Json::objectValue);
  res["id"] = channelId.ToHex ();
  res["playername"] = playerName;
  res["existsonchain"] = exists;

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

  return res;
}

Json::Value
ChannelManager::ToJson () const
{
  std::lock_guard<std::mutex> lock(mut);
  return UnlockedToJson ();
}

void
ChannelManager::NotifyStateChange () const
{
  /* Callers are expected to hold a lock on mut already, as is the case in
     all the updating code that will call here anyway.  */
  VLOG (1) << "Notifying waiting threads about state change...";
  cvStateChanged.notify_all ();
}

Json::Value
ChannelManager::WaitForChange () const
{
  std::unique_lock<std::mutex> lock(mut);

  VLOG (1) << "Waiting for state change on condition variable...";
  cvStateChanged.wait (lock);
  VLOG (1) << "Potential state change detected in WaitForChange";

  return UnlockedToJson ();
}

} // namespace xaya
