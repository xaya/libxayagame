// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_TESTUTILS_HPP
#define XAYASHIPS_TESTUTILS_HPP

#include "grid.hpp"
#include "logic.hpp"

#include <gamechannel/signatures.hpp>
#include <gamechannel/testutils.hpp>
#include <xayagame/sqlitestorage.hpp>
#include <xayagame/testutils.hpp>

#include <json/json.h>

#include <gtest/gtest.h>

#include <string>

namespace ships
{

/**
 * Parses a string into JSON.
 */
Json::Value ParseJson (const std::string& str);

/**
 * Test fixture that creates a ShipsLogic instance with an in-memory database
 * for testing of the on-chain GSP code.  It also includes a mock RPC server
 * for signature verification.
 */
class InMemoryLogicFixture : public testing::Test
{

private:

  /**
   * Helper class that is essentially a ShipsLogic but using a mock
   * signature verifier rather than the RPC one.
   */
  class ShipsLogicWithVerifier : public ShipsLogic
  {

  private:

    /** The verifier used.  */
    const xaya::SignatureVerifier& verifier;

  protected:

    const xaya::SignatureVerifier&
    GetSignatureVerifier () override
    {
      return verifier;
    }

  public:

    explicit ShipsLogicWithVerifier (const xaya::SignatureVerifier& v)
      : verifier(v)
    {}

  };

protected:

  xaya::MockSignatureVerifier verifier;
  ShipsLogicWithVerifier game;

  /**
   * Initialises the test case.  This connects the game instance to an
   * in-memory database and sets up the schema on it.
   */
  InMemoryLogicFixture ();

  /**
   * Returns the raw database handle of the test game.
   */
  xaya::SQLiteDatabase& GetDb ();

  /**
   * Returns our board rules.
   */
  const ShipsBoardRules& GetBoardRules () const;

};

} // namespace ships

#endif // XAYASHIPS_TESTUTILS_HPP
