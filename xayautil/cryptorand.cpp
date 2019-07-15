// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cryptorand.hpp"

#include <glog/logging.h>

#include <openssl/rand.h>

namespace xaya
{

template <>
  uint256
  CryptoRand::Get<uint256> ()
{
  unsigned char bytes[uint256::NUM_BYTES];
  CHECK_EQ (RAND_bytes (bytes, uint256::NUM_BYTES), 1);

  uint256 res;
  res.FromBlob (bytes);

  return res;
}

} // namespace xaya
