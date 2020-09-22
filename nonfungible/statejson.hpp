// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_STATEJSON_HPP
#define NONFUNGIBLE_STATEJSON_HPP

#include "assets.hpp"

#include <xayagame/sqlitestorage.hpp>

#include <json/json.h>

#include <string>

namespace nf
{

/**
 * Wrapper around a (read-only) database, which is able to extract bits
 * of the game state as JSON.  This is basically the internal implementation
 * of the RPC interface, but without the actual RPC server around and in
 * an easily-testable form.
 */
class StateJsonExtractor
{

private:

  /** The underlying database.  */
  const xaya::SQLiteDatabase& db;

public:

  explicit StateJsonExtractor (const xaya::SQLiteDatabase& d)
    : db(d)
  {}

  StateJsonExtractor () = delete;
  StateJsonExtractor (const StateJsonExtractor&) = delete;
  void operator= (const StateJsonExtractor&) = delete;

  /**
   * Retrieves an "overview list" of all assets in the system.
   */
  Json::Value ListAssets () const;

  /**
   * Retrieves detailed data about the given asset.  This includes the
   * custom string and also a list of all holders / balances.
   */
  Json::Value GetAssetDetails (const Asset& a) const;

  /**
   * Retrieves a single balance of a (user, asset) combination.
   */
  Json::Value GetBalance (const Asset& a, const std::string& name) const;

  /**
   * Returns all assets and balances owned by the given user.
   */
  Json::Value GetUserBalances (const std::string& name) const;

  /**
   * Returns the entire game state as JSON.  This method is not very efficient
   * and might produce a huge result, and should thus be avoided in practice.
   * It is mainly meant for testing, e.g. on regtest.
   */
  Json::Value FullState () const;

};

} // namespace nf

#endif // NONFUNGIBLE_STATEJSON_HPP
