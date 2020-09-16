// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_TESTUTILS_HPP
#define NONFUNGIBLE_TESTUTILS_HPP

#include "xayagame/sqlitestorage.hpp"

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <json/json.h>

namespace nf
{

/**
 * Parses JSON from a string.
 */
Json::Value ParseJson (const std::string& val);

/**
 * Test fixture with a temporary, in-memory SQLite database and our
 * database schema applied.
 */
class DBTest : public testing::Test
{

private:

  xaya::SQLiteDatabase db;

protected:

  DBTest ();

  /**
   * Returns the underlying database handle for SQLite.
   */
  sqlite3*
  GetHandle ()
  {
    return *db;
  }

  /**
   * Returns a Database instance for the test.
   */
  xaya::SQLiteDatabase&
  GetDb ()
  {
    return db;
  }

};

} // namespace nf

#endif // NONFUNGIBLE_TESTUTILS_HPP
