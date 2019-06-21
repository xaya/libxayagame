// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coord.hpp"

#include <gtest/gtest.h>

namespace ships
{
namespace
{

using DirectionTests = testing::Test;

TEST_F (DirectionTests, ValidInversion)
{
  EXPECT_EQ (-Direction::RIGHT, Direction::LEFT);
  EXPECT_EQ (Direction::RIGHT, -Direction::LEFT);

  EXPECT_EQ (-Direction::UP, Direction::DOWN);
  EXPECT_EQ (Direction::UP, -Direction::DOWN);
}

TEST_F (DirectionTests, InvalidInversion)
{
  EXPECT_DEATH (-static_cast<Direction> (42), "Invalid direction");
}

using CoordTests = testing::Test;

TEST_F (CoordTests, Comparison)
{
  EXPECT_EQ (Coord (5, 2), Coord (5, 2));
  EXPECT_NE (Coord (5, 2), Coord (5, 3));
  EXPECT_NE (Coord (5, 2), Coord (4, 2));
}

TEST_F (CoordTests, FromIndex)
{
  EXPECT_EQ (Coord (0), Coord (0, 0));
  EXPECT_EQ (Coord (5), Coord (0, 5));
  EXPECT_EQ (Coord (8), Coord (1, 0));
  EXPECT_EQ (Coord (60), Coord (7, 4));
  EXPECT_EQ (Coord (63), Coord (7, 7));
}

TEST_F (CoordTests, GetIndex)
{
  for (int ind = 0; ind < Coord::CELLS; ++ind)
    EXPECT_EQ (Coord (ind).GetIndex (), ind);
}

TEST_F (CoordTests, IsOnBoard)
{
  EXPECT_TRUE (Coord (0, 0).IsOnBoard ());
  EXPECT_TRUE (Coord (7, 7).IsOnBoard ());
  for (int ind = 0; ind < Coord::CELLS; ++ind)
    EXPECT_TRUE (Coord (ind).IsOnBoard ());

  EXPECT_FALSE (Coord (0, 8).IsOnBoard ());
  EXPECT_FALSE (Coord (8, 0).IsOnBoard ());
  EXPECT_FALSE (Coord (-1, 0).IsOnBoard ());
  EXPECT_FALSE (Coord (0, -1).IsOnBoard ());
  EXPECT_FALSE (Coord (8, -1).IsOnBoard ());
  EXPECT_FALSE (Coord (-1, 8).IsOnBoard ());
  EXPECT_FALSE (Coord (-1, -1).IsOnBoard ());
  EXPECT_FALSE (Coord (-1, -1).IsOnBoard ());

  EXPECT_FALSE (Coord (-1).IsOnBoard ());
  EXPECT_FALSE (Coord (-1000).IsOnBoard ());
  EXPECT_FALSE (Coord (64).IsOnBoard ());
  EXPECT_FALSE (Coord (1000000).IsOnBoard ());
}

TEST_F (CoordTests, PlusDirection)
{
  EXPECT_EQ (Coord (5, 2) + Direction::LEFT, Coord (5, 1));
  EXPECT_EQ (Coord (5, 2) + Direction::RIGHT, Coord (5, 3));
  EXPECT_EQ (Coord (5, 2) + Direction::UP, Coord (4, 2));
  EXPECT_EQ (Coord (5, 2) + Direction::DOWN, Coord (6, 2));

  EXPECT_EQ (Coord (2, 0) + Direction::LEFT, Coord (2, -1));
  EXPECT_EQ (Coord (2, 7) + Direction::RIGHT, Coord (2, 8));
  EXPECT_EQ (Coord (0, 2) + Direction::UP, Coord (-1, 2));
  EXPECT_EQ (Coord (7, 2) + Direction::DOWN, Coord (8, 2));
}

TEST_F (CoordTests, MinusDirection)
{
  EXPECT_EQ (Coord (5, 2) - Direction::LEFT, Coord (5, 3));
  EXPECT_EQ (Coord (5, 2) - Direction::RIGHT, Coord (5, 1));
  EXPECT_EQ (Coord (5, 2) - Direction::UP, Coord (6, 2));
  EXPECT_EQ (Coord (5, 2) - Direction::DOWN, Coord (4, 2));
}

TEST_F (CoordTests, PlusMinusInvalidDirection)
{
  const Direction invalid = static_cast<Direction> (42);
  EXPECT_DEATH (Coord (5, 2) + invalid, "Invalid direction");
  EXPECT_DEATH (Coord (5, 2) - invalid, "Invalid direction");
}

} // anonymous namespace
} // namespace ships
