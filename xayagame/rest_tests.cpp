// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rest.hpp"

#include <gtest/gtest.h>

namespace xaya
{

class RestApiTests : public testing::Test
{

protected:

  static bool
  MatchEndpoint (const std::string& path, const std::string& endpoint,
                 std::string& remainder)
  {
    return RestApi::MatchEndpoint (path, endpoint, remainder);
  }

};

namespace
{

TEST_F (RestApiTests, MatchEndpoint)
{
  std::string remainder;
  EXPECT_FALSE (MatchEndpoint ("", "/foo", remainder));
  EXPECT_FALSE (MatchEndpoint ("/bar", "/foo", remainder));

  ASSERT_TRUE (MatchEndpoint ("", "", remainder));
  EXPECT_EQ (remainder, "");

  ASSERT_TRUE (MatchEndpoint ("/foo", "/foo", remainder));
  EXPECT_EQ (remainder, "");

  ASSERT_TRUE (MatchEndpoint ("/foo/bla", "/foo/", remainder));
  EXPECT_EQ (remainder, "bla");
}

} // anonymous namespace
} // namespace xaya
