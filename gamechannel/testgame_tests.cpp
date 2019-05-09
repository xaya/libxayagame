// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include "proto/metadata.pb.h"

#include <gtest/gtest.h>

namespace xaya
{
namespace
{

class AdditionRulesTests : public testing::Test
{

protected:

  ChannelMetadata meta;
  AdditionRules rules;

};

TEST_F (AdditionRulesTests, CompareStates)
{
  EXPECT_TRUE (rules.CompareStates (meta, "1 2", " 1 2 "));
  EXPECT_TRUE (rules.CompareStates (meta, "105 10", "105 10"));
  EXPECT_FALSE (rules.CompareStates (meta, "2 1", "3 1"));
  EXPECT_FALSE (rules.CompareStates (meta, "105 1", "106 1"));
  EXPECT_FALSE (rules.CompareStates (meta, "5 1", "5 2"));
}

TEST_F (AdditionRulesTests, WhoseTurn)
{
  EXPECT_EQ (rules.WhoseTurn (meta, "13 1"), 1);
  EXPECT_EQ (rules.WhoseTurn (meta, "42 1"), 0);
  EXPECT_EQ (rules.WhoseTurn (meta, "99 2"), 1);
  EXPECT_EQ (rules.WhoseTurn (meta, "100 10"), BoardRules::NO_TURN);
  EXPECT_EQ (rules.WhoseTurn (meta, "105 10"), BoardRules::NO_TURN);
}

TEST_F (AdditionRulesTests, TurnCount)
{
  EXPECT_EQ (rules.TurnCount (meta, "10 12"), 12);
  EXPECT_EQ (rules.TurnCount (meta, "105 1"), 1);
}

TEST_F (AdditionRulesTests, ApplyMove)
{
  BoardState newState;
  ASSERT_TRUE (rules.ApplyMove (meta, "42 5", "13", newState));
  EXPECT_EQ (newState, "55 6");
  ASSERT_TRUE (rules.ApplyMove (meta, "99 10", "2", newState));
  EXPECT_EQ (newState, "101 11");

  EXPECT_FALSE (rules.ApplyMove (meta, "42 1", "0", newState));
  EXPECT_FALSE (rules.ApplyMove (meta, "42 1", "-1", newState));

  EXPECT_DEATH (rules.ApplyMove (meta, "100 1", "1", newState), "no turn");
}

} // anonymous namespace
} // namespace xaya
