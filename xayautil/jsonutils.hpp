// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_JSONUTILS_HPP
#define XAYAUTIL_JSONUTILS_HPP

#include <json/json.h>

#include <cstdint>

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

/**
 * Tries to parse a (non-negative) CHI amount from JSON, e.g. from what the
 * block data contains for moves, or even Xaya Core's RPC interface directly.
 * Returns true on success and fills in the amount as number of satoshis in
 * this case.  If the value is invalid (e.g. larger than the actual CHI
 * supply), false is returned instead.
 */
bool ChiAmountFromJson (const Json::Value& val, int64_t& sat);

/**
 * Converts a CHI amount given as number of satoshis to a JSON value,
 * e.g. for interacting with Xaya Core's JSON-RPC interface.
 */
Json::Value ChiAmountToJson (int64_t sat);

} // namespace xaya

#endif // XAYAUTIL_JSONUTILS_HPP
