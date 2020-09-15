// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include "schema.hpp"

#include <glog/logging.h>

namespace nf
{

DBTest::DBTest ()
  : db("test", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY)
{
  SetupDatabaseSchema (GetHandle ());
}

} // namespace nf
