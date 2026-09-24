// Copyright (C) 2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_UTF8_HPP
#define XAYAUTIL_UTF8_HPP

#include <string>

namespace xaya
{

/**
 * Checks if the given string is valid UTF-8 (if interpreted as a sequence
 * of bytes) as per the definition in RFC 3629.  Returns true only if the
 * entire string is well-formed UTF-8.
 *
 * This accepts all Unicode scalar values, including control characters
 * (e.g. bytes 0x00-0x1F).  It rejects overlong sequences, surrogate-pair
 * characters, codepoints above U+10FFFF and invalid byte sequences.
 */
bool IsValidUtf8 (const std::string& data);

} // namespace xaya

#endif // XAYAUTIL_UTF8_HPP
