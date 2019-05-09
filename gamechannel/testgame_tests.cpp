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
  EXPECT_TRUE (rules.CompareStates (meta, "1", " 1 "));
  EXPECT_TRUE (rules.CompareStates (meta, "105", "105"));
  EXPECT_FALSE (rules.CompareStates (meta, "2", "3"));
  EXPECT_FALSE (rules.CompareStates (meta, "105", "106"));
}

TEST_F (AdditionRulesTests, WhoseTurn)
{
  EXPECT_EQ (rules.WhoseTurn (meta, "13"), 1);
  EXPECT_EQ (rules.WhoseTurn (meta, "42"), 0);
  EXPECT_EQ (rules.WhoseTurn (meta, "99"), 1);
  EXPECT_EQ (rules.WhoseTurn (meta, "100"), BoardRules::NO_TURN);
  EXPECT_EQ (rules.WhoseTurn (meta, "105"), BoardRules::NO_TURN);
}

TEST_F (AdditionRulesTests, ApplyMove)
{
  BoardState newState;
  ASSERT_TRUE (rules.ApplyMove (meta, "42", "13", newState));
  EXPECT_EQ (newState, "55");
  ASSERT_TRUE (rules.ApplyMove (meta, "99", "2", newState));
  EXPECT_EQ (newState, "101");

  EXPECT_FALSE (rules.ApplyMove (meta, "42", "0", newState));
  EXPECT_FALSE (rules.ApplyMove (meta, "42", "-1", newState));

  EXPECT_DEATH (rules.ApplyMove (meta, "100", "1", newState), "no turn");
}

} // anonymous namespace
} // namespace xaya
