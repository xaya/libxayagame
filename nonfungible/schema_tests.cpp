// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "schema.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>

namespace nf
{
namespace
{

using SchemaTests = DBTest;

TEST_F (SchemaTests, Valid)
{
  /* DBTest itself already sets up the schema.  */
}

TEST_F (SchemaTests, MultipleTimesIsOk)
{
  SetupDatabaseSchema (GetDb ());
  SetupDatabaseSchema (GetDb ());
}

} // anonymous namespace
} // namespace nf
