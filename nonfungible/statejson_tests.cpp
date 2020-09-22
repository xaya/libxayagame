// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "statejson.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>

namespace nf
{
namespace
{

class StateJsonTests : public DBTest
{

protected:

  const StateJsonExtractor ext;

  StateJsonTests ()
    : ext(GetDb ())
  {}

};

TEST_F (StateJsonTests, ListAssets)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertAsset (Asset ("domob", "bar"), "null");
  InsertAsset (Asset ("andy", "xyz"), "null");
  InsertBalance (Asset ("domob", "foo"), "daniel", 50);

  EXPECT_EQ (ext.ListAssets (), ParseJson (R"([
    {"m": "andy", "a": "xyz"},
    {"m": "domob", "a": "bar"},
    {"m": "domob", "a": "foo"}
  ])"));
}

TEST_F (StateJsonTests, GetAssetDetails)
{
  InsertAsset (Asset ("domob", "foo"), "data");
  InsertAsset (Asset ("domob", "bar"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 2);
  InsertBalance (Asset ("domob", "foo"), "andy", 5);

  EXPECT_EQ (ext.GetAssetDetails (Asset ("domob", "foo")), ParseJson (R"({
    "asset": {"m": "domob", "a": "foo"},
    "data": "data",
    "supply": 7,
    "balances": {"andy": 5, "domob": 2}
  })"));

  EXPECT_EQ (ext.GetAssetDetails (Asset ("domob", "bar")), ParseJson (R"({
    "asset": {"m": "domob", "a": "bar"},
    "data": null,
    "supply": 0,
    "balances": {}
  })"));
}

TEST_F (StateJsonTests, GetBalance)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 2);
  InsertBalance (Asset ("domob", "foo"), "andy", 5);

  EXPECT_EQ (ext.GetBalance (Asset ("domob", "foo"), "domob"),
             AmountToJson (2));
  EXPECT_EQ (ext.GetBalance (Asset ("domob", "foo"), "andy"),
             AmountToJson (5));
  EXPECT_EQ (ext.GetBalance (Asset ("domob", "foo"), "other"),
             AmountToJson (0));
  EXPECT_EQ (ext.GetBalance (Asset ("domob", "bar"), "domob"),
             AmountToJson (0));
}

TEST_F (StateJsonTests, GetUserBalances)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertAsset (Asset ("domob", "bar"), "null");
  InsertAsset (Asset ("andy", "xyz"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 2);
  InsertBalance (Asset ("andy", "xyz"), "domob", 1);
  InsertBalance (Asset ("domob", "foo"), "andy", 5);

  EXPECT_EQ (ext.GetUserBalances ("domob"), ParseJson (R"([
    {"asset": {"m": "andy", "a": "xyz"}, "balance": 1},
    {"asset": {"m": "domob", "a": "foo"}, "balance": 2}
  ])"));

  EXPECT_EQ (ext.GetUserBalances ("andy"), ParseJson (R"([
    {"asset": {"m": "domob", "a": "foo"}, "balance": 5}
  ])"));

  EXPECT_EQ (ext.GetUserBalances ("other"), ParseJson ("[]"));
}

TEST_F (StateJsonTests, FullState)
{
  InsertAsset (Asset ("domob", "foo"), "data");
  InsertAsset (Asset ("domob", "bar"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 2);
  InsertBalance (Asset ("domob", "foo"), "andy", 5);

  EXPECT_EQ (ext.FullState (), ParseJson (R"([
    {
      "asset": {"m": "domob", "a": "bar"},
      "data": null,
      "supply": 0,
      "balances": {}
    },
    {
      "asset": {"m": "domob", "a": "foo"},
      "data": "data",
      "supply": 7,
      "balances": {"andy": 5, "domob": 2}
    }
  ])"));
}

} // anonymous namespace
} // namespace nf
