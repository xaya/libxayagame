// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "schema.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>

namespace ships
{

class SchemaTests : public InMemoryLogicFixture
{

protected:

  /**
   * Exposes SetupSchema from ShipsLogic.
   */
  void
  SetupSchema ()
  {
    game.SetupSchema (*GetDb ());
  }

};

namespace
{

TEST_F (SchemaTests, Valid)
{
  /* To verify that the database schema is valid, we do not need to do anything
     in the test specifically.  The InMemoryLogicFixture already sets up the
     schema, so that this would fail if it is invalid.  */
}

TEST_F (SchemaTests, TwiceOk)
{
  SetupSchema ();
  SetupSchema ();
}

} // anonymous namespace
} // namespace ships
