// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelgame.hpp"

#include "schema.hpp"
#include "stateproof.hpp"

#include <glog/logging.h>

namespace xaya
{

void
ChannelGame::SetupGameChannelsSchema (sqlite3* db)
{
  InternalSetupGameChannelsSchema (db);
}

namespace
{

/**
 * Verifies if the given state proof is valid and for a state later
 * than (in turn count) the current on-chain state.  If the turn count is the
 * same, then a dispute being applied counts as "later" than a previous
 * non-disputed state.  This is logic in common between ProcessDispute and
 * ProcessResolution.
 */
bool
CheckStateProofIsLater (XayaRpcClient& rpc, const BoardRules& rules,
                        const ChannelData& ch, const proto::StateProof& proof,
                        const bool provenIsDispute, BoardState& provenState)
{
  const auto& id = ch.GetId ();
  const auto& meta = ch.GetMetadata ();
  const auto& onChainState = ch.GetState ();

  if (!VerifyStateProof (rpc, rules, id, meta, onChainState, proof,
                         provenState))
    {
      LOG (WARNING) << "Dispute/resolution has invalid state proof";
      return false;
    }

  const auto onChainParsed = rules.ParseState (id, meta, onChainState);
  CHECK (onChainParsed != nullptr);
  const auto provenParsed = rules.ParseState (id, meta, provenState);
  CHECK (provenParsed != nullptr);

  const bool onChainIsDispute = ch.HasDispute ();
  const unsigned onChainCnt = onChainParsed->TurnCount ();
  const unsigned provenCnt = provenParsed->TurnCount ();

  if (provenCnt > onChainCnt)
    return true;
  if (provenCnt == onChainCnt && !onChainIsDispute && provenIsDispute)
    {
      LOG (INFO)
          << "Allowing dispute to be filed for non-disputed on-chain state"
             " of the same turn count " << provenCnt;
      return true;
    }

  LOG (WARNING)
      << "Dispute/resolution has turn count " << provenCnt
      << ", which is not beyond the on-chain count " << onChainCnt;
  return false;
}

} // anonymous namespace

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

  BoardState provenState;
  if (!CheckStateProofIsLater (GetXayaRpc (), rules, ch, proof,
                               true, provenState))
    return false;

  const auto provenParsed = rules.ParseState (id, meta, provenState);
  CHECK (provenParsed != nullptr);
  if (provenParsed->WhoseTurn () == ParsedBoardState::NO_TURN)
    {
      LOG (WARNING) << "Cannot file dispute for 'no turn' situation";
      return false;
    }

  VLOG (1) << "Dispute is valid, updating state...";

  ch.SetState (provenState);
  ch.SetDisputeHeight (height);

  return true;
}

bool
ChannelGame::ProcessResolution (ChannelData& ch, const proto::StateProof& proof)
{
  BoardState provenState;
  if (!CheckStateProofIsLater (GetXayaRpc (), GetBoardRules (), ch, proof,
                               false, provenState))
    return false;

  VLOG (1) << "Resolution is valid, updating state...";

  ch.SetState (provenState);
  ch.ClearDispute ();

  return true;
}

} // namespace xaya
