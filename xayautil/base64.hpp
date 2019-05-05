// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_BASE64_HPP
#define XAYAUTIL_BASE64_HPP

#include <string>

namespace xaya
{

/**
 * Encodes the given string (potentially with binary data) into OpenSSL's
 * base64 format.
 */
std::string EncodeBase64 (const std::string& data);

/**
 * Decodes the given string from base64 format to a string of binary data.
 * Returns false if the decoding failed because of invalid data.
 */
bool DecodeBase64 (const std::string& encoded, std::string& data);

} // namespace xaya

#endif // XAYAUTIL_BASE64_HPP
