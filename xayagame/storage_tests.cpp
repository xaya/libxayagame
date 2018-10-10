// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "storage_tests.hpp"

namespace xaya
{
namespace
{

INSTANTIATE_TYPED_TEST_CASE_P (Memory, BasicStorageTests, MemoryStorage);
INSTANTIATE_TYPED_TEST_CASE_P (Memory, PruningStorageTests, MemoryStorage);

} // anonymous namespace
} // namespace xaya
