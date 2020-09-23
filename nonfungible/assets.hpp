// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_ASSETS_HPP
#define NONFUNGIBLE_ASSETS_HPP

#include <sqlite3.h>

#include <json/json.h>

#include <cstdint>
#include <ostream>
#include <string>

namespace nf
{

/** Type used for amounts.  */
using Amount = int64_t;

/** Maximum valid amount value (i.e. largest total supply of an asset).  */
static constexpr Amount MAX_AMOUNT = (1ll << 60);

/**
 * Converts an amount to JSON.
 */
Json::Value AmountToJson (Amount n);

/**
 * Parses an amount from JSON and validates the range.  Returns true on success.
 */
bool AmountFromJson (const Json::Value& val, Amount& n);

/**
 * An asset (specified by minter and name).
 */
class Asset
{

private:

  /** The minter's name (without p/ prefix).  */
  std::string minter;

  /** The asset's name.  */
  std::string name;

public:

  Asset () = default;

  explicit Asset (const std::string& m, const std::string& n)
    : minter(m), name(n)
  {}

  Asset (const Asset&) = default;
  Asset& operator= (const Asset&) = default;

  const std::string&
  GetMinter () const
  {
    return minter;
  }

  /**
   * Binds an asset to two parameters in the SQLite statement.
   */
  void BindToParams (sqlite3_stmt* stmt, int indMinter, int indName) const;

  /**
   * Converts the asset to a JSON value.
   */
  Json::Value ToJson () const;

  /**
   * Extracts an Asset value from a database result.
   */
  static Asset FromColumns (sqlite3_stmt* stmt, int indMinter, int indName);

  /**
   * Checks if the given string as a valid asset name.
   */
  static bool IsValidName (const std::string& str);

  /**
   * Tries to parse an asset from JSON into this instance.
   * Returns true if successful.
   */
  bool FromJson (const Json::Value& val);

  friend bool
  operator== (const Asset& a, const Asset& b)
  {
    return a.minter == b.minter && a.name == b.name;
  }

  friend bool
  operator!= (const Asset& a, const Asset& b)
  {
    return !(a == b);
  }

  friend bool
  operator< (const Asset& a, const Asset& b)
  {
    if (a.minter != b.minter)
      return a.minter < b.minter;
    return a.name < b.name;
  }

  /**
   * Writes out the asset to a stream.  The format is readable and suited
   * for normal debugging / logging, and not meant to be fully precise and
   * unambiguous if the minter or asset name is weird in any case.  It is
   * also not consensus-relevant in any way.
   */
  friend std::ostream& operator<< (std::ostream& out, const Asset& a);

};

} // namespace nf

#endif // NONFUNGIBLE_ASSETS_HPP
