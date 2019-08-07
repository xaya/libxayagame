// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "moves.hpp"

#include <gtest/gtest.h>

#include <sstream>

namespace mover
{
namespace
{

class ParseMoveTests : public testing::Test
{

protected:

  void
  ExpectValid (const std::string& jsonStr, const proto::Direction expectedDir,
               const unsigned expectedSteps) const
  {
    Json::Value move;
    std::istringstream in(jsonStr);
    in >> move;

    proto::Direction dir;
    unsigned steps;
    ASSERT_TRUE (ParseMove (move, dir, steps));
    EXPECT_EQ (dir, expectedDir);
    EXPECT_EQ (steps, expectedSteps);
  }

  void
  ExpectInvalid (const std::string& jsonStr) const
  {
    Json::Value move;
    std::istringstream in(jsonStr);
    in >> move;

    proto::Direction dir;
    unsigned steps;
    EXPECT_FALSE (ParseMove (move, dir, steps));
  }

};

TEST_F (ParseMoveTests, ValidMinimalSteps)
{
  ExpectValid (R"(
    {
      "d": "k",
      "n": 1
    }
  )", proto::UP, 1);
}

TEST_F (ParseMoveTests, ValidMaximalSteps)
{
  ExpectValid (R"(
    {
      "n": 1000000,
      "d": "b"
    }
  )", proto::LEFT_DOWN, 1000000);
}

TEST_F (ParseMoveTests, InvalidNoObject)
{
  ExpectInvalid ("[]");
  ExpectInvalid ("\"a\"");
  ExpectInvalid ("42");
}

TEST_F (ParseMoveTests, InvalidWrongKeys)
{
  ExpectInvalid ("{}");
  ExpectInvalid (R"(
    {
      "n": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k"
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "x": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 42,
      "x": 42
    }
  )");
}

TEST_F (ParseMoveTests, InvalidDirection)
{
  ExpectInvalid (R"(
    {
      "d": 42,
      "n": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "kk",
      "n": 42
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "x",
      "n": 42
    }
  )");
}

TEST_F (ParseMoveTests, InvalidSteps)
{
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": "k"
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 0
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 2.5
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": -1
    }
  )");
  ExpectInvalid (R"(
    {
      "d": "k",
      "n": 1000001
    }
  )");
}

} // anonymous namespace
} // namespace mover
