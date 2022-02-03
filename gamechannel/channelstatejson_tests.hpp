// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelstatejson.hpp"

#include "testgame.hpp"

#include <xayautil/hash.hpp>

namespace xaya
{

/**
 * Checks if the given actual game-state JSON for a channel matches the
 * expected one, taking into account potential differences in protocol buffer
 * serialisation for the metadata and stateproof.  Those are verified by
 * comparing the protocol buffers themselves.
 */
void
CheckChannelJson (Json::Value actual, const std::string& expected,
                  const uint256& id, const proto::ChannelMetadata& meta,
                  const BoardState& reinitState,
                  const BoardState& proofState);

class ChannelStateJsonTests : public TestGameFixture
{

protected:

  /** First test channel.  */
  const uint256 id1 = SHA256::Hash ("channel 1");

  /** Metadata for channel 1.  */
  proto::ChannelMetadata meta1;

  /** Second test channel.  */
  const uint256 id2 = SHA256::Hash ("channel 2");

  /** Metadata for channel 2.  */
  proto::ChannelMetadata meta2;

  ChannelStateJsonTests ();

};

} // anonymous namespace
