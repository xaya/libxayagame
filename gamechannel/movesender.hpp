// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_MOVESENDER_HPP
#define GAMECHANNEL_MOVESENDER_HPP

#include "openchannel.hpp"
#include "proto/stateproof.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayautil/uint256.hpp>

#include <json/writer.h>

#include <string>

namespace xaya
{

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

  /** Xaya RPC connection to use.  */
  XayaRpcClient& rpc;

  /** Xaya wallet RPC that we use.  */
  XayaWalletRpcClient& wallet;

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
                       XayaRpcClient& r, XayaWalletRpcClient& w,
                       OpenChannel& oc);

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
   * Sends a dispute based on the given state proof.  Returns the transaction
   * ID (or null if the transaction failed).
   */
  uint256 SendDispute (const proto::StateProof& proof);

  /**
   * Sends a resolution based on the given state proof.  Returns the
   * transaction ID (or null if the transaction failed).
   */
  uint256 SendResolution (const proto::StateProof& proof);

  /**
   * Checks if a name_update transaction from this MoveSender with the
   * given txid is in the node's mempool.  This can be used to check if
   * an equivalent move is still pending before requesting to send
   * another one.
   */
  bool IsPending (const uint256& txid) const;

};

} // namespace xaya

#endif // GAMECHANNEL_MOVESENDER_HPP
