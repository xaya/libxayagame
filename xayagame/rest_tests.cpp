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

  using SuccessResult = RestApi::SuccessResult;

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

TEST_F (RestApiTests, Compression)
{
  /* We do not yet have any decompression methods, so we just verify that
     "it works" to compress some data.  */
  const SuccessResult original("text/plain", std::string (1 << 20, 'x'));
  const auto compressed = original.Gzip ();
  EXPECT_EQ (compressed.GetType (), "text/plain+gzip");
  EXPECT_LT (compressed.GetPayload ().size (), original.GetPayload ().size ());
}

} // anonymous namespace
} // namespace xaya
