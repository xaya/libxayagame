// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_RANDOM_HPP
#define XAYAGAME_RANDOM_HPP

#include "uint256.hpp"

namespace xaya
{

/**
 * Handle for generating deterministic "random" numbers based off an
 * initial seed.
 */
class Random
{

private:

  /**
   * The current state / seed.  The bytes of the seed are given out one by
   * one as random numbers.  When it runs out, then a next seed is computed
   * by hashing the previous one.
   */
  uint256 seed;

  /** Index of the next byte to give out for the current seed.  */
  unsigned nextIndex;

public:

  /**
   * Constructs an empty instance that is not yet seeded.  It must not be
   * used to extract any random bytes before Seed() has been called.
   */
  Random ();

  Random (const Random&) = delete;
  void operator= (const Random&) = delete;

  /**
   * Sets / replaces the seed with the given value.
   */
  void Seed (const uint256& s);

  /**
   * Extracts the next byte or perhaps other type (e.g. uint32_t).
   */
  template <typename T>
    T Next ();

};

} // namespace xaya

#endif // XAYAGAME_RANDOM_HPP
