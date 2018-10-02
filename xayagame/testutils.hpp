// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_TESTUTILS_HPP
#define XAYAGAME_TESTUTILS_HPP

/* Shared utility functions for unit tests of xayagame.  */

#include "uint256.hpp"

namespace xaya
{

/**
 * Returns a uint256 based on the given number, to be used as block hashes
 * in tests.
 */
uint256 BlockHash (unsigned num);

} // namespace xaya

#endif // XAYAGAME_TESTUTILS_HPP
