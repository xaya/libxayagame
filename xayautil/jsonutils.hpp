// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_JSONUTILS_HPP
#define XAYAUTIL_JSONUTILS_HPP

#include <json/json.h>

namespace xaya
{

/**
 * Returns true if the given JSON value is a true integer, i.e. really
 * was parsed from an integer literal.  This is in contrast to a value that
 * has isInt() return true, but was actually parsed from a floating-point
 * literal and just happens to be integral.
 *
 * This function can be used in conjunction with isInt / isUInt on JSON values
 * if we want to enforce that they are passed as integer literals.
 */
bool IsIntegerValue (const Json::Value& val);

} // namespace xaya

#endif // XAYAUTIL_JSONUTILS_HPP
