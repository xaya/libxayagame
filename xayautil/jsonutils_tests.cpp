// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "jsonutils.hpp"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <sstream>

namespace xaya
{
namespace
{

/** Number of satoshis in one CHI.  */
constexpr int64_t COIN = 100'000'000;

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

using JsonChiAmountTests = testing::Test;

TEST_F (JsonChiAmountTests, AmountToJson)
{
  const Json::Value val = ChiAmountToJson (COIN);
  ASSERT_TRUE (val.isDouble ());
  ASSERT_EQ (val.asDouble (), 1.0);
}

TEST_F (JsonChiAmountTests, ValidAmountFromString)
{
  struct Test
  {
    std::string str;
    int64_t expected;
  };
  const Test tests[] =
    {
      {"0", 0},
      {"1.5", 3 * COIN / 2},
      {"0.1", COIN / 10},
      {"30.0", 30 * COIN},
      {"70123456.12345678", 7'012'345'612'345'678},
    };

  for (const auto& t : tests)
    {
      LOG (INFO) << "Testing: " << t.str;
      int64_t actual;
      ASSERT_TRUE (ChiAmountFromJson (ParseJson (t.str), actual));
      EXPECT_EQ (actual, t.expected);
    }
}

TEST_F (JsonChiAmountTests, ValidAmountRoundtrip)
{
  constexpr int64_t MAX_AMOUNT = 80'000'000 * COIN;
  const int64_t testValues[] = {
      0, 1,
      COIN - 1, COIN, COIN + 1,
      MAX_AMOUNT - 1, MAX_AMOUNT
  };
  for (const int64_t a : testValues)
    {
      LOG (INFO) << "Testing with amount " << a;
      const Json::Value val = ChiAmountToJson (a);
      int64_t a2;
      ASSERT_TRUE (ChiAmountFromJson (val, a2));
      EXPECT_EQ (a2, a);
    }
}

TEST_F (JsonChiAmountTests, InvalidAmountFromJson)
{
  for (const auto& str : {"{}", "\"foo\"", "true", "-0.1", "80000000.1"})
    {
      int64_t a;
      EXPECT_FALSE (ChiAmountFromJson (ParseJson (str), a));
    }
}

} // anonymous namespace
} // namespace xaya
