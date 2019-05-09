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
 * than (in turn count) the current on-chain state.  This is logic
 * in common between ProcessDispute and ProcessResolution.
 */
bool
CheckStateProofIsLater (XayaRpcClient& rpc, const BoardRules& rules,
                        const ChannelData& ch, const StateProof& proof,
                        BoardState& provenState)
{
  const auto& meta = ch.GetMetadata ();
  const auto& onChainState = ch.GetState ();

  if (!VerifyStateProof (rpc, rules, meta, onChainState, proof, provenState))
    {
      LOG (WARNING) << "Dispute/resolution has invalid state proof";
      return false;
    }

  const unsigned onChainCnt = rules.TurnCount (meta, onChainState);
  const unsigned provenCnt = rules.TurnCount (meta, provenState);
  if (onChainCnt >= provenCnt)
    {
      LOG (WARNING)
          << "Dispute/resolution has turn count " << provenCnt
          << ", which is not beyond the on-chain count " << onChainCnt;
      return false;
    }
  CHECK_GT (provenCnt, onChainCnt);

  return true;
}

} // anonymous namespace

bool
ChannelGame::ProcessDispute (ChannelData& ch, const unsigned height,
                             const StateProof& proof)
{
  BoardState provenState;
  if (!CheckStateProofIsLater (GetXayaRpc (), GetBoardRules (), ch, proof,
                               provenState))
    return false;

  /* If there is already a dispute in the on-chain game state, then it can only
     have been placed there by an earlier block (or perhaps the same block
     in edge cases).  */
  if (ch.HasDispute ())
    CHECK_GE (height, ch.GetDisputeHeight ());

  VLOG (1) << "Dispute is valid, updating state...";

  ch.SetState (provenState);
  ch.SetDisputeHeight (height);

  return true;
}

bool
ChannelGame::ProcessResolution (ChannelData& ch, const StateProof& proof)
{
  BoardState provenState;
  if (!CheckStateProofIsLater (GetXayaRpc (), GetBoardRules (), ch, proof,
                               provenState))
    return false;

  VLOG (1) << "Resolution is valid, updating state...";

  ch.SetState (provenState);
  ch.ClearDispute ();

  return true;
}

} // namespace xaya
