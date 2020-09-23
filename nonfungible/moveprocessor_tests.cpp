// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "moveprocessor.hpp"

#include "dbutils.hpp"
#include "testutils.hpp"

#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>

namespace nf
{
namespace
{

/* ************************************************************************** */

/**
 * Type for a list of expected assets in the database.  The values are
 * the custom data strings.  The magic value "null" means that they
 * are null.
 */
using AllAssets = std::map<Asset, std::string>;

/**
 * Expects that the set of assets in the database matches exactly
 * the given expected set.
 */
void
ExpectAssets (const xaya::SQLiteDatabase& db, const AllAssets& expected)
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `minter`, `asset`, `data`
      FROM `assets`
  )");

  AllAssets actual;
  while (StepStatement (stmt))
    {
      const auto a = Asset::FromColumns (stmt, 0, 1);
      std::string data;
      if (ColumnIsNull (stmt, 2))
        data = "null";
      else
        data = ColumnExtract<std::string> (stmt, 2);

      const auto ins = actual.emplace (a, data);
      CHECK (ins.second) << "Already had entry for " << a;
    }

  ASSERT_EQ (actual, expected);
}

/**
 * Type for a list of expected balances in the database.  The first map
 * is keyed by account names, the second by assets.
 */
using AllBalances = std::map<std::string, std::map<Asset, Amount>>;

/**
 * Expects that the set of balances in the database matches exactly
 * the given expected set.
 */
void
ExpectBalances (const xaya::SQLiteDatabase& db, const AllBalances& expected)
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `name`, `minter`, `asset`, `balance`
      FROM `balances`
  )");

  AllBalances actual;
  while (StepStatement (stmt))
    {
      const auto name = ColumnExtract<std::string> (stmt, 0);
      const auto a = Asset::FromColumns (stmt, 1, 2);
      const auto num = ColumnExtract<int64_t> (stmt, 3);

      auto& nameEntry = actual[name];
      const auto ins = nameEntry.emplace (a, num);
      CHECK (ins.second) << "Already had entry for " << name << " and " << a;
    }

  ASSERT_EQ (actual, expected);
}

class MoveProcessorTests : public DBTest
{

protected:

  /**
   * Processes a move given as JSON string for the given name.
   */
  void
  Process (const std::string& name, const std::string& str)
  {
    Json::Value mv(Json::objectValue);
    mv["name"] = name;
    mv["move"] = ParseJson (str);

    Json::Value moves(Json::arrayValue);
    moves.append (mv);

    MoveProcessor proc(GetDb ());
    proc.ProcessAll (moves);
  }

};

/* ************************************************************************** */

TEST_F (MoveProcessorTests, ValidMint)
{
  Process ("domob", R"([
    {"m": {"a": "foo", "n": 20}},
    {"m": {"a": "äöü", "n": 1, "d": ""}}
  ])");
  Process ("andy", R"(
    {"m": {"a": "foo", "n": 0, "d": "custom\u0000data"}}
  )");
  Process ("", R"(
    {"m": {"a": "", "n": 10}}
  )");

  ExpectAssets (GetDb (), {
    {Asset ("domob", "foo"), "null"},
    {Asset ("domob", u8"äöü"), ""},
    {Asset ("andy", "foo"), std::string ("custom\0data", 11)},
    {Asset ("", ""), "null"},
  });
  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 20}, {Asset ("domob", u8"äöü"), 1}}},
    {"", {{Asset ("", ""), 10}}},
  });
}

TEST_F (MoveProcessorTests, InvalidMintFormat)
{
  Process ("domob", R"([
    {"m": "foo"},
    {"m": {"n": 20}},
    {"m": {"n": 20, "x": 10}},
    {"m": {"a": 42, "n": 20}},
    {"m": {"a": "foo"}},
    {"m": {"a": "foo", "x": 10}},
    {"m": {"a": "foo\nbar", "n": 20}},
    {"m": {"a": "foo\nbar", "n": "20"}},
    {"m": {"a": "foo", "n": "20"}},
    {"m": {"a": "foo", "n": -20}},
    {"m": {"a": "foo", "n": 20, "x": 10}},
    {"m": {"a": "foo", "n": 20, "d": "data", "x": 10}},
    {"m": {"a": "foo", "n": 20, "d": ["foo"]}}
  ])");

  ExpectAssets (GetDb (), {});
  ExpectBalances (GetDb (), {});
}

TEST_F (MoveProcessorTests, InvalidMintExistingAsset)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  Process ("domob", R"(
    {"m": {"a": "foo", "n": 20}}
  )");

  ExpectAssets (GetDb (), {
    {{Asset ("domob", "foo"), "null"}},
  });
  ExpectBalances (GetDb (), {});
}

TEST_F (MoveProcessorTests, MintSupply)
{
  Process ("domob", R"([
    {"m": {"a": "zero", "n": 0}},
    {"m": {"a": "max",        "n": 1152921504606846976}},
    {"m": {"a": "toomuch",    "n": 1152921504606846977}},
    {"m": {"a": "superlarge", "n": 9999999999999999999999999999999999}}
  ])");

  ExpectAssets (GetDb (), {
    {Asset ("domob", "zero"), "null"},
    {Asset ("domob", "max"), "null"},
  });
  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "max"), MAX_AMOUNT}}},
  });
}

/* ************************************************************************** */

TEST_F (MoveProcessorTests, ValidTransfer)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertAsset (Asset ("domob", "bar"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);
  InsertBalance (Asset ("domob", "foo"), "andy", 10);
  InsertBalance (Asset ("domob", "bar"), "domob", 20);

  Process ("domob", R"([
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 5, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 5, "r": "domob"}},
    {"t": {"a": {"m": "domob", "a": "bar"}, "n": 10, "r": ""}},
    {"t": {"a": {"m": "domob", "a": "bar"}, "n": 10, "r": "invalid\nxaya"}}
  ])");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 5}}},
    {"andy", {{Asset ("domob", "foo"), 15}}},
    {"", {{Asset ("domob", "bar"), 10}}},
    {"invalid\nxaya", {{Asset ("domob", "bar"), 10}}},
  });
}

TEST_F (MoveProcessorTests, InvalidTransferFormat)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);

  Process ("domob", R"([
    {"t": "foo"},
    {"t": {"n": 1, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": "andy", "x": null}},
    {"t": {"a": {"m": "domob", "a": "bar"}, "n": 1, "r": "andy"}},
    {"t": {"a": {"m": "domob"}, "n": 1, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": "1", "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": -1, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 0, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": 50}}
  ])");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 10}}},
  });
}

TEST_F (MoveProcessorTests, InvalidTransferTooMuch)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);

  Process ("domob", R"([
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 5, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 6, "r": "daniel"}}
  ])");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 5}}},
    {"andy", {{Asset ("domob", "foo"), 5}}},
  });
}

/* ************************************************************************** */

TEST_F (MoveProcessorTests, ValidBurn)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);
  InsertBalance (Asset ("domob", "foo"), "andy", 10);

  Process ("domob", R"([
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 6}},
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 2}}
  ])");
  Process ("andy", R"([
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 10}}
  ])");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 2}}},
  });
}

TEST_F (MoveProcessorTests, InvalidBurnFormat)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);

  Process ("domob", R"([
    {"b": "foo"},
    {"b": {"n": 1}},
    {"b": {"a": {"m": "domob", "a": "foo"}}},
    {"b": {"a": "foo", "n": 1}},
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 1, "x": "foo"}},
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": -1}},
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": "1"}},
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 0}}
  ])");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 10}}},
  });
}

TEST_F (MoveProcessorTests, InvalidBurnTooMuch)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);

  Process ("domob", R"([
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 5}},
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 6}}
  ])");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 5}}},
  });
}

/* ************************************************************************** */

TEST_F (MoveProcessorTests, MoveJsonTypes)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);

  Process ("domob", R"([
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": "andy"}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": "andy"}}
  ])");
  Process ("domob", R"(
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": "andy"}}
  )");
  Process ("domob", "[]");
  Process ("domob", "null");
  Process ("domob", "false");
  Process ("domob", "42");
  Process ("domob", "\"foo\"");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 7}}},
    {"andy", {{Asset ("domob", "foo"), 3}}},
  });
}

TEST_F (MoveProcessorTests, ProcessedInOrder)
{
  Process ("domob", R"([
    {"m": {"a": "foo", "n": 20}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 10, "r": "andy"}},
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 10}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": "andy"}}
  ])");

  ExpectAssets (GetDb (), {
    {Asset ("domob", "foo"), "null"},
  });
  ExpectBalances (GetDb (), {
    {"andy", {{Asset ("domob", "foo"), 10}}},
  });
}

TEST_F (MoveProcessorTests, InvalidArrayElementsIgnored)
{
  InsertAsset (Asset ("domob", "foo"), "null");
  InsertBalance (Asset ("domob", "foo"), "domob", 10);

  Process ("domob", R"([
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 5, "r": "andy"}},
    "foo",
    {"b": {"a": {"m": "domob", "a": "foo"}, "n": 6}},
    {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": "andy"}}
  ])");

  ExpectBalances (GetDb (), {
    {"domob", {{Asset ("domob", "foo"), 4}}},
    {"andy", {{Asset ("domob", "foo"), 6}}},
  });
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace nf
