// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include "proto/metadata.pb.h"

#include <xayautil/hash.hpp>

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace xaya
{
namespace
{

class AdditionRulesTests : public TestGameFixture
{

protected:

  const uint256 channelId = SHA256::Hash ("foo");
  proto::ChannelMetadata meta;

  bool
  CompareStates (const BoardState& a, const BoardState& b)
  {
    const auto pa = game.rules.ParseState (channelId, meta, a);
    CHECK (pa != nullptr);

    return pa->Equals (b);
  }

  int
  WhoseTurn (const BoardState& s)
  {
    const auto p = game.rules.ParseState (channelId, meta, s);
    CHECK (p != nullptr);

    return p->WhoseTurn ();
  }

  unsigned
  TurnCount (const BoardState& s)
  {
    const auto p = game.rules.ParseState (channelId, meta, s);
    CHECK (p != nullptr);

    return p->TurnCount ();
  }

  bool
  ApplyMove (const BoardState& old, const BoardMove& mv, BoardState& newState)
  {
    const auto po = game.rules.ParseState (channelId, meta, old);
    CHECK (po != nullptr);

    return po->ApplyMove (mockXayaServer.GetClient (), mv, newState);
  }

};

TEST_F (AdditionRulesTests, ParseInvalid)
{
  EXPECT_EQ (game.rules.ParseState (channelId, meta, "invalid"), nullptr);
}

TEST_F (AdditionRulesTests, CompareStates)
{
  EXPECT_TRUE (CompareStates ("1 2", " 1 2 "));
  EXPECT_TRUE (CompareStates ("105 10", "105 10"));
  EXPECT_FALSE (CompareStates ("2 1", "3 1"));
  EXPECT_FALSE (CompareStates ("105 1", "106 1"));
  EXPECT_FALSE (CompareStates ("5 1", "5 2"));
  EXPECT_FALSE (CompareStates ("5 1", "invalid"));
}

TEST_F (AdditionRulesTests, WhoseTurn)
{
  EXPECT_EQ (WhoseTurn ("13 1"), 1);
  EXPECT_EQ (WhoseTurn ("42 1"), 0);
  EXPECT_EQ (WhoseTurn ("99 2"), 1);
  EXPECT_EQ (WhoseTurn ("100 10"), ParsedBoardState::NO_TURN);
  EXPECT_EQ (WhoseTurn ("105 10"), ParsedBoardState::NO_TURN);
}

TEST_F (AdditionRulesTests, TurnCount)
{
  EXPECT_EQ (TurnCount ("10 12"), 12);
  EXPECT_EQ (TurnCount ("105 1"), 1);
}

TEST_F (AdditionRulesTests, ApplyMove)
{
  BoardState newState;
  ASSERT_TRUE (ApplyMove ("42 5", "13", newState));
  EXPECT_EQ (newState, "55 6");
  ASSERT_TRUE (ApplyMove ("99 10", "2", newState));
  EXPECT_EQ (newState, "101 11");
}

TEST_F (AdditionRulesTests, ApplyMoveInvalid)
{
  BoardState newState;
  EXPECT_FALSE (ApplyMove ("42 1", "0", newState));
  EXPECT_FALSE (ApplyMove ("42 1", "-1", newState));
}

class AdditionChannelTests : public TestGameFixture
{

private:

  const uint256 channelId = SHA256::Hash ("foo");
  proto::ChannelMetadata meta;

protected:

  /**
   * Calls MaybeAutoMove after parsing the given state.
   */
  bool
  MaybeAutoMove (const BoardState& state, BoardMove& mv)
  {
    const auto p = game.rules.ParseState (channelId, meta, state);
    CHECK (p != nullptr);
    return game.channel.MaybeAutoMove (*p, mv);
  }

};

TEST_F (AdditionChannelTests, AutoMoves)
{
  BoardMove mv;

  EXPECT_FALSE (MaybeAutoMove ("5 0", mv));
  EXPECT_FALSE (MaybeAutoMove ("30 0", mv));

  ASSERT_TRUE (MaybeAutoMove ("6 5", mv));
  EXPECT_EQ (mv, "2");

  ASSERT_TRUE (MaybeAutoMove ("17 5", mv));
  EXPECT_EQ (mv, "2");

  ASSERT_TRUE (MaybeAutoMove ("88 5", mv));
  EXPECT_EQ (mv, "2");

  ASSERT_TRUE (MaybeAutoMove ("99 5", mv));
  EXPECT_EQ (mv, "2");

  ASSERT_TRUE (MaybeAutoMove ("108 5", mv));
  EXPECT_EQ (mv, "2");

  game.channel.SetAutomovesEnabled (false);
  EXPECT_FALSE (MaybeAutoMove ("8 0", mv));
}

} // anonymous namespace
} // namespace xaya
