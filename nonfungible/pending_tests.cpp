// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pending.hpp"

#include "testutils.hpp"

#include <glog/logging.h>
#include <gtest/gtest.h>

namespace nf
{
namespace
{

/* ************************************************************************** */

class PendingStateTests : public testing::Test
{

protected:

  PendingState state;

  /** Utility variable to use as output for GetBalance.  */
  Amount balance;

  /**
   * Expects that the JSON of the current pending state matches a value
   * parsed from string.
   */
  void
  ExpectStateJson (const std::string& expectedStr) const
  {
    ASSERT_EQ (state.ToJson (), ParseJson (expectedStr));
  }

};

TEST_F (PendingStateTests, Empty)
{
  EXPECT_FALSE (state.IsNewAsset (Asset ("domob", "foo")));
  EXPECT_FALSE (state.GetBalance (Asset ("domob", "foo"), "domob", balance));

  ExpectStateJson (R"({
    "assets": [],
    "balances": {}
  })");
}

TEST_F (PendingStateTests, Assets)
{
  static const std::string data = "data";
  state.AddAsset (Asset ("domob", "foo"), nullptr);
  state.AddAsset (Asset ("domob", "bar"), &data);

  EXPECT_TRUE (state.IsNewAsset (Asset ("domob", "foo")));
  EXPECT_TRUE (state.IsNewAsset (Asset ("domob", "bar")));
  EXPECT_FALSE (state.IsNewAsset (Asset ("domob", "baz")));
  EXPECT_FALSE (state.IsNewAsset (Asset ("andy", "foo")));

  ExpectStateJson (R"({
    "balances": {},
    "assets":
      [
        {"asset": {"m": "domob", "a": "bar"}, "data": "data"},
        {"asset": {"m": "domob", "a": "foo"}, "data": null}
      ]
  })");
}

TEST_F (PendingStateTests, Balances)
{
  state.SetBalance (Asset ("domob", "foo"), "andy", 10);
  state.SetBalance (Asset ("domob", "bar"), "andy", 10);
  state.SetBalance (Asset ("domob", "foo"), "domob", 20);
  state.SetBalance (Asset ("domob", "bar"), "andy", 30);

  ASSERT_TRUE (state.GetBalance (Asset ("domob", "foo"), "domob", balance));
  EXPECT_EQ (balance, 20);

  ASSERT_TRUE (state.GetBalance (Asset ("domob", "foo"), "andy", balance));
  EXPECT_EQ (balance, 10);

  ASSERT_TRUE (state.GetBalance (Asset ("domob", "bar"), "andy", balance));
  EXPECT_EQ (balance, 30);

  ExpectStateJson (R"({
    "assets": [],
    "balances":
      {
        "andy":
          [
            {"asset": {"m": "domob", "a": "bar"}, "balance": 30},
            {"asset": {"m": "domob", "a": "foo"}, "balance": 10}
          ],
        "domob":
          [
            {"asset": {"m": "domob", "a": "foo"}, "balance": 20}
          ]
      }
  })");
}

/* ************************************************************************** */

class PendingStateUpdaterTests : public DBTest
{

private:

  PendingState state;

protected:

  /**
   * Processes the given move on top of our state.
   */
  void
  Process (const std::string& name, const std::string& str)
  {
    Json::Value mv(Json::objectValue);
    mv["name"] = name;
    mv["move"] = ParseJson (str);

    PendingStateUpdater proc(GetDb (), state);
    proc.ProcessOne (mv);
  }

  /**
   * Expects that the JSON of the current pending state matches a value
   * parsed from string.
   */
  void
  ExpectStateJson (const std::string& expectedStr) const
  {
    ASSERT_EQ (state.ToJson (), ParseJson (expectedStr));
  }

};

TEST_F (PendingStateUpdaterTests, Minting)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertAsset (Asset ("andy", "bar"), "null");

  Process ("domob", R"([
    {"m": {"a": "foo", "n": 10}},
    {"m": {"a": "bar", "n": 10}},
    {"m": {"a": "bar", "n": 20, "d": "invalid"}},
    {"m": {"a": "baz", "n": 30, "d": "valid"}},
    {"m": {"a": "zero", "n": 0}}
  ])");

  ExpectStateJson (R"({
    "assets":
      [
        {"asset": {"m": "domob", "a": "bar"}, "data": null},
        {"asset": {"m": "domob", "a": "baz"}, "data": "valid"},
        {"asset": {"m": "domob", "a": "zero"}, "data": null}
      ],
    "balances":
      {
        "domob":
          [
            {"asset": {"m": "domob", "a": "bar"}, "balance": 10},
            {"asset": {"m": "domob", "a": "baz"}, "balance": 30}
          ]
      }
  })");
}

TEST_F (PendingStateUpdaterTests, Transfer)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "andy", 20);

  /* These are not touched and should not show up in the pending state.  */
  InsertAsset (Asset ("domob", "bar"), "null");
  InsertBalance (Asset ("domob", "bar"), "andy", 42);

  Process ("andy", R"([
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 10, "r": "domob"}},
    {"t": {"a": {"m": "andy", "a": "mine"}, "n": 1, "r": "wrong"}},
    {"m": {"a": "mine", "n": 10}},
    {"t": {"a": {"m": "andy", "a": "mine"}, "n": 1, "r": "domob"}},
    {"t": {"a": {"m": "andy", "a": "mine"}, "n": 10, "r": "wrong"}}
  ])");

  ExpectStateJson (R"({
    "assets":
      [
        {"asset": {"m": "andy", "a": "mine"}, "data": null}
      ],
    "balances":
      {
        "andy":
          [
            {"asset": {"m": "andy", "a": "mine"}, "balance": 9},
            {"asset": {"m": "domob", "a": "foo"}, "balance": 10}
          ],
        "domob":
          [
            {"asset": {"m": "andy", "a": "mine"}, "balance": 1},
            {"asset": {"m": "domob", "a": "foo"}, "balance": 10}
          ]
      }
  })");
}

TEST_F (PendingStateUpdaterTests, Burn)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 20);

  Process ("domob", R"([
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 2}},
    {"b": {"a": {"m": "domob", "a": "bar"}, "n": 10}},
    {"m": {"a": "bar", "n": 10}},
    {"b": {"a": {"m": "domob", "a": "bar"}, "n": 6}},
    {"b": {"a": {"m": "domob", "a": "bar"}, "n": 6}}
  ])");

  ExpectStateJson (R"({
    "assets":
      [
        {"asset": {"m": "domob", "a": "bar"}, "data": null}
      ],
    "balances":
      {
        "domob":
          [
            {"asset": {"m": "domob", "a": "bar"}, "balance": 4},
            {"asset": {"m": "domob", "a": "foo"}, "balance": 18}
          ]
      }
  })");
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace nf
