// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelgame.hpp"

#include "protoutils.hpp"
#include "protoversion.hpp"
#include "schema.hpp"
#include "stateproof.hpp"

#include <xayautil/hash.hpp>

#include <glog/logging.h>

namespace xaya
{

/* ************************************************************************** */

void
ChannelGame::SetupGameChannelsSchema (SQLiteDatabase& db)
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
  const auto& rules = GetBoardRules ();

  if (!CheckVersionedProto (rules, meta, proof))
    return false;

  BoardState provenState;
  if (!VerifyStateProof (GetSignatureVerifier (), rules, GetGameId (), id,
                         meta, ch.GetReinitState (), proof, provenState))
    {
      LOG (WARNING) << "Dispute has invalid state proof";
      return false;
    }

  const auto& onChainState = ch.GetLatestState ();
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
  const auto& rules = GetBoardRules ();

  if (!CheckVersionedProto (rules, meta, proof))
    return false;

  BoardState provenState;
  if (!VerifyStateProof (GetSignatureVerifier (), rules, GetGameId (), id,
                         meta, ch.GetReinitState (), proof, provenState))
    {
      LOG (WARNING) << "Resolution has invalid state proof";
      return false;
    }

  const auto onChainParsed = rules.ParseState (id, meta, ch.GetLatestState ());
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

const SignatureVerifier&
ChannelGame::GetSignatureVerifier ()
{
  if (verifier == nullptr)
    verifier = std::make_unique<RpcSignatureVerifier> (GetXayaRpc ());

  CHECK (verifier != nullptr);
  return *verifier;
}

/* ************************************************************************** */

void
ChannelGame::PendingMoves::Clear ()
{
  channels.clear ();
}

void
ChannelGame::PendingMoves::AddPendingStateProof (ChannelData& ch,
                                                 const proto::StateProof& proof)
{
  ChannelGame& game = dynamic_cast<ChannelGame&> (GetSQLiteGame ());

  const auto& id = ch.GetId ();
  const auto& meta = ch.GetMetadata ();
  const auto& rules = game.GetBoardRules ();

  if (!CheckVersionedProto (rules, meta, proof))
    return;

  BoardState provenState;
  if (!VerifyStateProof (game.GetSignatureVerifier (), rules, GetGameId (), id,
                         meta, ch.GetReinitState (), proof, provenState))
    {
      LOG (WARNING) << "StateProof of pending move is invalid";
      return;
    }

  const auto provenParsed = rules.ParseState (id, meta, provenState);
  CHECK (provenParsed != nullptr);
  const unsigned provenCnt = provenParsed->TurnCount ();
  VLOG (1)
      << "Found valid pending state proof for channel " << id.ToHex ()
      << " with turn count " << provenCnt;

  const auto mit = channels.find (id);
  if (mit == channels.end ())
    {
      const auto onChainParsed = rules.ParseState (id, meta,
                                                   ch.GetLatestState ());
      CHECK (onChainParsed != nullptr);
      const unsigned onChainCnt = onChainParsed->TurnCount ();
      VLOG (1) << "On-chain turn count: " << onChainCnt;
      if (provenCnt > onChainCnt)
        {
          LOG (INFO)
              << "Found new latest state for channel " << id.ToHex ()
              << " in pending move with turn count " << provenCnt;
          channels.emplace (id, PendingChannelData ({proof, provenCnt}));
        }
    }
  else
    {
      PendingChannelData& pending = mit->second;
      VLOG (1) << "Previous pending turn count: " << pending.turnCount;
      if (provenCnt > pending.turnCount)
        {
          LOG (INFO)
              << "Found new latest state for channel " << id.ToHex ()
              << " in pending move with turn count " << provenCnt;
          pending.proof = proof;
          pending.turnCount = provenCnt;
        }
    }
}

Json::Value
ChannelGame::PendingMoves::ToJson () const
{
  Json::Value channelsJson(Json::objectValue);
  for (const auto& entry : channels)
    {
      Json::Value cur(Json::objectValue);
      cur["id"] = entry.first.ToHex ();
      cur["proof"] = ProtoToBase64 (entry.second.proof);
      cur["turncount"] = entry.second.turnCount;
      channelsJson[entry.first.ToHex ()] = cur;
    }

  Json::Value res(Json::objectValue);
  res["channels"] = channelsJson;

  return res;
}

void
UpdateMetadataReinit (const uint256& mvid, proto::ChannelMetadata& meta)
{
  SHA256 hasher;
  hasher << meta.reinit ();
  hasher << mvid;

  meta.set_reinit (hasher.Finalise ().GetBinaryString ());
}

/* ************************************************************************** */

} // namespace xaya
