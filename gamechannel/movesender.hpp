// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_MOVESENDER_HPP
#define GAMECHANNEL_MOVESENDER_HPP

#include "openchannel.hpp"
#include "proto/stateproof.pb.h"

#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayautil/uint256.hpp>

#include <json/writer.h>

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

/**
 * A connection to the Xaya wallet that allows sending moves (mainly
 * disputes and resolutions from ChannelManager, but also game-specific code
 * may use it e.g. for winner statements).  They are sent by name_update's
 * to a given player name.
 *
 * The actual format for dispute and resolution moves is game-dependent,
 * and construction of the moves is done through the game's implementation
 * of OpenChannel.
 */
class MoveSender
{

private:

  /** Xaya wallet RPC that we use.  */
  XayaWalletRpcClient& rpc;

  /** OpenChannel instance for building moves.  */
  OpenChannel& game;

  /** ID of the game channel this is for.  */
  const uint256 channelId;

  /** The Xaya name that should be updated (including p/).  */
  const std::string playerName;

  /** The game ID for constructing moves.  */
  const std::string gameId;

  /**
   * Builder for JSON serialisation, with the options configured as we want
   * them (i.e. to avoid any unnecessary whitespace).
   */
  Json::StreamWriterBuilder jsonWriterBuilder;

public:

  explicit MoveSender (const std::string& gId,
                       const uint256& chId, const std::string& nm,
                       XayaWalletRpcClient& w, OpenChannel& oc);

  virtual ~MoveSender () = default;

  MoveSender () = delete;
  MoveSender (const MoveSender&) = delete;
  void operator= (const MoveSender&) = delete;

  /**
   * Sends the given JSON value as move.  This is used for the implementations
   * of SendDispute and SendResolution, and it can also be used by game-specific
   * logic for sending other moves (e.g. submitting a winner statement).
   *
   * Returns the txid if successful and a null uint256 if an error occurred
   * and the move could not be sent.
   */
  uint256 SendMove (const Json::Value& mv);

  /**
   * Sends a dispute based on the given state proof.
   */
  virtual void SendDispute (const proto::StateProof& proof);

  /**
   * Sends a resolution based on the given state proof.
   */
  virtual void SendResolution (const proto::StateProof& proof);

};

} // namespace xaya

#endif // GAMECHANNEL_MOVESENDER_HPP
