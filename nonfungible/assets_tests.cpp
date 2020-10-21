// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets.hpp"

#include "dbutils.hpp"
#include "testutils.hpp"

#include <glog/logging.h>
#include <gtest/gtest.h>

namespace nf
{
namespace
{

using AssetsTests = DBTest;

TEST_F (AssetsTests, AmountJsonRoundtrip)
{
  static constexpr Amount tests[] = {0, MAX_AMOUNT, 42, 5000};
  for (const auto a : tests)
    {
      const Json::Value val = AmountToJson (a);
      ASSERT_TRUE (val.isInt64 ());

      Amount recovered;
      ASSERT_TRUE (AmountFromJson (val, recovered));
      ASSERT_EQ (recovered, a);
    }
}

TEST_F (AssetsTests, InvalidAmountFromJson)
{
  Amount a;
  ASSERT_FALSE (AmountFromJson (Json::Value (MAX_AMOUNT + 1), a));
  for (const std::string str : {"-5", "42.0", "1e5", "null", "false", "\"10\"",
                                "[1]", "{\"foo\":\"bar\"}"})
    ASSERT_FALSE (AmountFromJson (ParseJson (str), a)) << str;
}

TEST_F (AssetsTests, ValidAmountFromJson)
{
  Amount a;

  ASSERT_TRUE (AmountFromJson (ParseJson ("42"), a));
  EXPECT_EQ (a, 42);

  ASSERT_TRUE (AmountFromJson (ParseJson ("1"), a));
  EXPECT_EQ (a, 1);

  ASSERT_TRUE (AmountFromJson (ParseJson ("-0"), a));
  EXPECT_EQ (a, 0);
}

TEST_F (AssetsTests, DatabaseRoundtrip)
{
  /* This asset is not actually valid (when parsed from JSON).  But that doesn't
     matter here, and makes sure that the DB logic can handle it.  */
  const Asset a(std::string (u8"äöü\0foo", 10), std::string ("bar\0baz", 7));

  auto stmt = GetDb ().Prepare (R"(
    INSERT INTO `assets`
      (`minter`, `asset`)
      VALUES (?1, ?2)
  )");
  a.BindToParams (*stmt, 1, 2);
  stmt.Execute ();

  stmt = GetDb ().PrepareRo (R"(
    SELECT `minter`, `asset` FROM `assets`
  )");

  CHECK (stmt.Step ());
  const Asset recovered = Asset::FromColumns (*stmt, 0, 1);
  EXPECT_EQ (recovered, a);
  CHECK (!stmt.Step ());
}

TEST_F (AssetsTests, IsValidName)
{
  EXPECT_TRUE (Asset::IsValidName (""));
  EXPECT_TRUE (Asset::IsValidName (" foo bar"));
  EXPECT_TRUE (Asset::IsValidName (u8"äöü"));
  EXPECT_FALSE (Asset::IsValidName ("foo\n"));
  EXPECT_FALSE (Asset::IsValidName (std::string ("foo\0", 4)));
}

TEST_F (AssetsTests, JsonRoundtrip)
{
  const std::string tests[] =
    {
      R"({"m": "domob", "a": "foo bar"})",
      u8R"({"m": "äöü", "a": "ß"})",
      R"({"m": "", "a": ""})",
    };

  for (const auto& t : tests)
    {
      const auto val = ParseJson (t);

      Asset a;
      ASSERT_TRUE (a.FromJson (val)) << val;

      EXPECT_EQ (a.ToJson (), val);
    }
}

TEST_F (AssetsTests, InvalidJson)
{
  const std::string tests[] =
    {
      "null",
      "{}",
      "[]",
      "\"foo\"",
      R"({"m": "foo", "a": "bar", "x": 42})",
      R"({"m": "foo"})",
      R"({"m": "foo", "a": "bar\nbaz"})",
      R"({"m": "foo", "a": "bar\u0000baz"})",
    };

  for (const auto& t : tests)
    {
      Asset a;
      ASSERT_FALSE (a.FromJson (ParseJson (t)));
    }
}

} // anonymous namespace
} // namespace nf
