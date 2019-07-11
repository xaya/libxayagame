// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_BROADCAST_HPP
#define GAMECHANNEL_BROADCAST_HPP

#include "proto/stateproof.pb.h"

#include <string>

namespace xaya
{

/**
 * Interface for a class that allows broadcasting moves / state-proofs
 * off-chain to the channel participants.  This is implemented by the
 * real-time implementations and mocked for testing.
 *
 * Functions of the interface may be called by different threads, but it is
 * guaranteed that only one thread at a time is accessing the instance.
 */
class OffChainBroadcast
{

protected:

  OffChainBroadcast () = default;

public:

  virtual ~OffChainBroadcast () = default;

  /**
   * Sends a new state (presumably after the player made a move) to all
   * channel participants.
   */
  virtual void SendNewState (const std::string& reinitId,
                             const proto::StateProof& proof) = 0;

};

} // namespace xaya

#endif // GAMECHANNEL_BROADCAST_HPP
