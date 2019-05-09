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

bool
ChannelGame::ProcessDispute (ChannelData& ch, const unsigned height,
                             const StateProof& proof)
{
  const auto& meta = ch.GetMetadata ();
  const auto& onChainState = ch.GetState ();
  const auto& rules = GetBoardRules ();

  BoardState provenState;
  if (!VerifyStateProof (GetXayaRpc (), rules, meta, onChainState, proof,
                         provenState))
    {
      LOG (WARNING) << "Dispute has invalid state proof";
      return false;
    }

  const unsigned onChainCnt = rules.TurnCount (meta, onChainState);
  const unsigned provenCnt = rules.TurnCount (meta, provenState);
  if (onChainCnt >= provenCnt)
    {
      LOG (WARNING)
          << "Dispute has turn count " << provenCnt
          << ", which is not beyond the on-chain count " << onChainCnt;
      return false;
    }
  CHECK_GT (provenCnt, onChainCnt);

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

} // namespace xaya
