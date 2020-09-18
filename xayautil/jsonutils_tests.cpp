// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "jsonutils.hpp"

#include <gtest/gtest.h>

#include <sstream>

namespace xaya
{
namespace
{

/**
 * Parses a string using the JSON parser and returns the value.
 */
Json::Value
ParseJson (const std::string& val)
{
  std::istringstream in(val);
  Json::Value res;
  in >> res;
  return res;
}

TEST (JsonUtilsTests, IsIntegerValue)
{
  EXPECT_TRUE (IsIntegerValue (ParseJson ("0")));
  EXPECT_TRUE (IsIntegerValue (ParseJson ("-5")));
  EXPECT_TRUE (IsIntegerValue (ParseJson ("42")));
  EXPECT_TRUE (IsIntegerValue (ParseJson ("18446744073709551615")));
  EXPECT_TRUE (IsIntegerValue (ParseJson ("-9223372036854775808")));

  EXPECT_FALSE (IsIntegerValue (ParseJson ("1e5")));
  EXPECT_FALSE (IsIntegerValue (ParseJson ("1.0")));
  EXPECT_FALSE (IsIntegerValue (ParseJson ("18446744073709551616")));
  EXPECT_FALSE (IsIntegerValue (ParseJson ("-9223372036854775809")));
}

} // anonymous namespace
} // namespace xaya
