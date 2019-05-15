// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "grid.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>

#include <glog/logging.h>

namespace ships
{
namespace
{

/* ************************************************************************** */

using GridTests = testing::Test;

TEST_F (GridTests, ConstructorGetBits)
{
  EXPECT_EQ (Grid ().GetBits (), 0);
  EXPECT_EQ (Grid (12345).GetBits (), 12345);
}

TEST_F (GridTests, BasicGetSet)
{
  Grid g(0x101);
  EXPECT_TRUE (g.Get (Coord (0)));
  EXPECT_TRUE (g.Get (Coord (8)));
  EXPECT_FALSE (g.Get (Coord (1)));
  EXPECT_FALSE (g.Get (Coord (63)));

  g.Set (Coord (5));
  g.Set (Coord (63));
  EXPECT_TRUE (g.Get (Coord (0)));
  EXPECT_TRUE (g.Get (Coord (5)));
  EXPECT_TRUE (g.Get (Coord (8)));
  EXPECT_FALSE (g.Get (Coord (4)));
  EXPECT_FALSE (g.Get (Coord (6)));
  EXPECT_FALSE (g.Get (Coord (62)));
  EXPECT_TRUE (g.Get (Coord (63)));
}

TEST_F (GridTests, ExhaustiveSet)
{
  for (int i = 0; i < Coord::CELLS; ++i)
    {
      Grid g;
      g.Set (Coord (i));
      for (int j = 0; j < Coord::CELLS; ++j)
        EXPECT_EQ (g.Get (Coord (j)), i == j);
    }
}

TEST_F (GridTests, CountOne)
{
  Grid g;
  EXPECT_EQ (g.CountOnes (), 0);

  for (int i = 0; i < Coord::CELLS; ++i)
    {
      g.Set (Coord (i));
      EXPECT_EQ (g.CountOnes (), i + 1);
    }
}

TEST_F (GridTests, Blob)
{
  Grid g(0x102);
  g.Set (Coord (63));

  const std::string blob = g.Blob ();
  ASSERT_EQ (blob.size (), 8);
  EXPECT_EQ (blob[0], static_cast<char> (2));
  EXPECT_EQ (blob[1], static_cast<char> (1));
  EXPECT_EQ (blob[7], static_cast<char> (0x80));
}

TEST_F (GridTests, TotalShipCells)
{
  EXPECT_EQ (Grid::TotalShipCells (), 8 + 6 + 4);
}

/* ************************************************************************** */

class VerifyPositionForAnswersTests : public testing::Test
{

protected:

  Grid position;
  Grid targeted;
  Grid hits;

  bool
  Verify () const
  {
    return VerifyPositionForAnswers (position, targeted, hits);
  }

};

TEST_F (VerifyPositionForAnswersTests, Works)
{
  EXPECT_TRUE (Verify ());

  position.Set (Coord (1));
  position.Set (Coord (5));
  position.Set (Coord (10));
  EXPECT_TRUE (Verify ());

  targeted.Set (Coord (1));
  targeted.Set (Coord (2));
  targeted.Set (Coord (5));
  EXPECT_FALSE (Verify ());

  hits.Set (Coord (1));
  hits.Set (Coord (5));
  EXPECT_TRUE (Verify ());

  hits.Set (Coord (2));
  EXPECT_FALSE (Verify ());
}

TEST_F (VerifyPositionForAnswersTests, InvalidHits)
{
  hits.Set (Coord (10));
  targeted.Set (Coord (10));
  Verify ();

  hits.Set (Coord (20));
  EXPECT_DEATH (Verify (), "Hit positions are not a subset");
}

/* ************************************************************************** */

class VerifyPositionOfShipsTests : public testing::Test
{

protected:

  /**
   * Verifies a position using VerifyPositionOfShips, parsing the configuration
   * first from a string.  In the string, "." is for empty places and "x"
   * for ships.
   */
  static bool
  Verify (const std::string& str)
  {
    return VerifyPositionOfShips (GridFromString (str));
  }

};

TEST_F (VerifyPositionOfShipsTests, Valid)
{
  EXPECT_TRUE (Verify (
    "xx...x.x"
    ".....x.x"
    "xxxx...."
    "........"
    "........"
    "x.x....."
    "x.x....."
    "x.x...xx"
  ));
}

TEST_F (VerifyPositionOfShipsTests, InvalidPlacement)
{
  EXPECT_FALSE (Verify (
    "xx.....x"
    "..x....x"
    "..x....."
    "........"
    "....xxxx"
    "x.x....."
    "x.x....."
    "x.x...xx"
  ));

  EXPECT_FALSE (Verify (
    ".xx....x"
    "x......x"
    "x......."
    "........"
    "....xxxx"
    "x.x....."
    "x.x....."
    "x.x...xx"
  ));

  EXPECT_FALSE (Verify (
    "..xx...x"
    "x......x"
    "x......."
    "........"
    "....xxxx"
    "x.x..x.."
    "x.x..x.."
    "x.x....."
  ));

  EXPECT_FALSE (Verify (
    ".......x"
    "xx.....x"
    "x......."
    "........"
    "....xxxx"
    "x.x....."
    "x.x..x.."
    "x.x..x.."
  ));
}

TEST_F (VerifyPositionOfShipsTests, ShipTypes)
{
  EXPECT_FALSE (Verify (
    "xx.....x"
    "...x...x"
    "...x...."
    "x......."
    "....xxxx"
    "x.x....."
    "x.x....."
    "x.x...xx"
  ));

  EXPECT_FALSE (Verify (
    "xx.x...x"
    "...x...x"
    "........"
    "xxxxx..x"
    ".......x"
    "x.x....x"
    "x.x....x"
    "x.x.xx.."
  ));

  EXPECT_FALSE (Verify (
    ".....x.x"
    ".....x.x"
    "xxxx...."
    "........"
    "........"
    "x.x....."
    "x.x....."
    "x.x...xx"
  ));

  EXPECT_FALSE (Verify (
    "xx...x.x"
    ".....x.x"
    "xxxx...."
    ".......x"
    ".......x"
    "x.x....x"
    "x.x....."
    "x.x...xx"
  ));
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace ships
