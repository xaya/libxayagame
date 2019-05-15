// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_TESTUTILS_HPP
#define XAYASHIPS_TESTUTILS_HPP

#include "grid.hpp"

#include <string>

namespace ships
{

/**
 * Parses a ship position given as string and returns it.  The string must be
 * 64 characters long (and may be split into 8 lines in code), consisting only
 * of the characters "." for zeros and "x" for ones.
 */
Grid GridFromString (const std::string& str);

} // namespace ships

#endif // XAYASHIPS_TESTUTILS_HPP
