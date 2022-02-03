// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "broadcast.hpp"

#include "channelmanager.hpp"
#include "channelmanager_tests.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

using testing::IsEmpty;
using testing::UnorderedElementsAre;

class BroadcastTests : public ChannelManagerTestFixture
{

protected:

  MockOffChainBroadcast offChain;

  BroadcastTests ()
    : offChain(cm.GetChannelId ())
  {
    cm.SetOffChainBroadcast (offChain);
  }

};

TEST_F (BroadcastTests, Participants)
{
  ProcessOnChain ("0 0", ValidProof ("10 5"), 0);
  EXPECT_THAT (offChain.GetParticipants (),
               UnorderedElementsAre ("player", "other"));

  ProcessOnChainNonExistant ();
  EXPECT_THAT (offChain.GetParticipants (), IsEmpty ());
}

} // anonymous namespace
} // namespace xaya
