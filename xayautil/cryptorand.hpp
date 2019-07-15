// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_CRYPTORAND_HPP
#define XAYAUTIL_CRYPTORAND_HPP

#include "uint256.hpp"

namespace xaya
{

/**
 * Generator for secure random data, i.e. not deterministic like Random.
 * This can be used e.g. to generate hash commitments and salt values for
 * channel games.
 */
class CryptoRand
{

public:

  CryptoRand () = default;

  CryptoRand (const CryptoRand&) = delete;
  void operator= (const CryptoRand&) = delete;

  /**
   * Returns a random value of given type (e.g. uint256).
   */
  template <typename T>
    T Get ();

};

} // namespace xaya

#endif // XAYAUTIL_CRYPTORAND_HPP
