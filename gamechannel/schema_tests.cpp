// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "schema.hpp"

#include "testgame.hpp"

#include <gtest/gtest.h>

namespace xaya
{
namespace
{

using SchemaTests = TestGameFixture;

TEST_F (SchemaTests, Valid)
{
  /* To verify that the database schema is valid, we do not need to do anything
     in the test specifically.  The TestGameFixture already sets up the schema,
     so that this would fail if it is invalid.  */
}

TEST_F (SchemaTests, TwiceOk)
{
  InternalSetupGameChannelsSchema (GetDb ());
  InternalSetupGameChannelsSchema (GetDb ());
}

} // anonymous namespace
} // namespace xaya
