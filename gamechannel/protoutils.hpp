// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_PROTOUTILS_HPP
#define GAMECHANNEL_PROTOUTILS_HPP

#include <string>

namespace xaya
{

/**
 * Encodes a protocol buffer as base64 string (e.g. suitable for storing in
 * a JSON value).
 */
template <typename Proto>
  std::string ProtoToBase64 (const Proto& msg);

/**
 * Decodes a base64-encoded string into a protocol buffer.
 */
template <typename Proto>
  bool ProtoFromBase64 (const std::string& str, Proto& msg);

} // namespace xaya

#include "protoutils.tpp"

#endif // GAMECHANNEL_PROTOUTILS_HPP
