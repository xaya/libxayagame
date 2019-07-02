// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelgame.hpp"

#include "schema.hpp"
#include "stateproof.hpp"

#include <xayautil/hash.hpp>

#include <glog/logging.h>

namespace xaya
{

void
ChannelGame::SetupGameChannelsSchema (sqlite3* db)
{
  InternalSetupGameChannelsSchema (db);
}

bool
ChannelGame::ProcessDispute (ChannelData& ch, const unsigned height,
                             const proto::StateProof& proof)
{
  /* If there is already a dispute in the on-chain game state, then it can only
     have been placed there by an earlier block (or perhaps the same block
     in edge cases).  */
  if (ch.HasDispute ())
    CHECK_GE (height, ch.GetDisputeHeight ());

  const auto& id = ch.GetId ();
  const auto& meta = ch.GetMetadata ();
  const auto& onChainState = ch.GetLatestState ();
  const auto& rules = GetBoardRules ();

  BoardState provenState;
  if (!VerifyStateProof (GetXayaRpc (), rules, id, meta, onChainState, proof,
                         provenState))
    {
      LOG (WARNING) << "Dispute has invalid state proof";
      return false;
    }

  const auto onChainParsed = rules.ParseState (id, meta, onChainState);
  CHECK (onChainParsed != nullptr);
  const auto provenParsed = rules.ParseState (id, meta, provenState);
  CHECK (provenParsed != nullptr);

  if (provenParsed->WhoseTurn () == ParsedBoardState::NO_TURN)
    {
      LOG (WARNING) << "Cannot file dispute for 'no turn' situation";
      return false;
    }

  const unsigned onChainCnt = onChainParsed->TurnCount ();
  const unsigned provenCnt = provenParsed->TurnCount ();

  if (provenCnt > onChainCnt)
    {
      VLOG (1)
          << "Disputing on-chain state at " << onChainCnt
          << " with new state at turn count " << provenCnt;
      ch.SetStateProof (proof);
      ch.SetDisputeHeight (height);
      return true;
    }

  if (provenCnt < onChainCnt)
    {
      LOG (WARNING)
          << "Dispute with state at turn " << provenCnt
          << " is invalid, on-chain state is at " << onChainCnt;
      return false;
    }

  CHECK_EQ (provenCnt, onChainCnt);

  if (ch.HasDispute ())
    {
      LOG (WARNING)
          << "Dispute has same turn count (" << provenCnt << ") as"
          << " on-chain state, which is already disputed";
      return false;
    }

  if (!provenParsed->Equals (onChainState))
    {
      LOG (WARNING)
          << "Dispute has same turn count as on-chain state (" << provenCnt
          << "), but a differing state";
      return false;
    }

  VLOG (1) << "Disputing existing on-chain state at turn " << provenCnt;
  ch.SetDisputeHeight (height);
  return true;
}

bool
ChannelGame::ProcessResolution (ChannelData& ch, const proto::StateProof& proof)
{
  const auto& id = ch.GetId ();
  const auto& meta = ch.GetMetadata ();
  const auto& onChainState = ch.GetLatestState ();
  const auto& rules = GetBoardRules ();

  BoardState provenState;
  if (!VerifyStateProof (GetXayaRpc (), rules, id, meta, onChainState, proof,
                         provenState))
    {
      LOG (WARNING) << "Dispute has invalid state proof";
      return false;
    }

  const auto onChainParsed = rules.ParseState (id, meta, onChainState);
  CHECK (onChainParsed != nullptr);
  const auto provenParsed = rules.ParseState (id, meta, provenState);
  CHECK (provenParsed != nullptr);

  const unsigned onChainCnt = onChainParsed->TurnCount ();
  const unsigned provenCnt = provenParsed->TurnCount ();
  if (provenCnt <= onChainCnt)
    {
      LOG (WARNING)
          << "Resolution for state at turn " << provenCnt
          << " is invalid, on-chain state is already at " << onChainCnt;
      return false;
    }

  VLOG (1) << "Resolution is valid, updating state...";
  ch.SetStateProof (proof);
  ch.ClearDispute ();
  return true;
}

void
UpdateMetadataReinit (const uint256& txid, proto::ChannelMetadata& meta)
{
  SHA256 hasher;
  hasher << meta.reinit ();
  hasher << txid;

  meta.set_reinit (hasher.Finalise ().GetBinaryString ());
}

} // namespace xaya
