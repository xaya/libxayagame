// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELMANAGER_TESTS_HPP
#define GAMECHANNEL_CHANNELMANAGER_TESTS_HPP

#include "channelmanager.hpp"

#include "testgame.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

namespace xaya
{

/**
 * Constructs a state proof for the given state, signed by both players
 * (and thus valid).
 */
proto::StateProof ValidProof (const std::string& state);

class ChannelManagerTestFixture : public TestGameFixture
{

protected:

  const uint256 blockHash = SHA256::Hash ("block hash");
  const unsigned height = 42;

  const uint256 channelId = SHA256::Hash ("channel id");
  proto::ChannelMetadata meta;

  ChannelManager cm;

  ChannelManagerTestFixture ();
  ~ChannelManagerTestFixture ();

  /**
   * Processes an on-chain update with fixed block hash and height, our
   * metadata and for the given data state.
   */
  void ProcessOnChain (const BoardState& reinitState,
                       const proto::StateProof& proof,
                       unsigned dispHeight);

  /**
   * Processes an on-chain update without the channel for our fixed block height
   * and hash.
   */
  void ProcessOnChainNonExistant ();

  /**
   * Returns the manager's current block hash and height.
   */
  const uint256&
  GetOnChainBlock (unsigned& height) const
  {
    height = cm.onChainHeight;
    return cm.blockHash;
  }

  /**
   * Extracts the latest state from boardStates.
   */
  BoardState GetLatestState () const;

  /**
   * Exposes the boardStates member of our ChannelManager to subtests.
   */
  const RollingState&
  GetBoardStates () const
  {
    return cm.boardStates;
  }

  /**
   * Exposes the exists member to subtests.
   */
  bool
  GetExists () const
  {
    return cm.exists;
  }

  /**
   * Exposes the dispute member to subtests.
   */
  const ChannelManager::DisputeData*
  GetDispute () const
  {
    return cm.dispute.get ();
  }

};

} // namespace xaya

#endif // GAMECHANNEL_CHANNELMANAGER_TESTS_HPP
