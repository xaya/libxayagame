// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_RANDOM_HPP
#define XAYAUTIL_RANDOM_HPP

#include "uint256.hpp"

#include <vector>

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

  /**
   * Returns a random integer i with 0 <= i < n based on this instance's
   * random number stream.
   */
  uint32_t NextInt (uint32_t n);

  /**
   * Performs a random roll and returns true with probability numer/denom.
   */
  bool ProbabilityRoll (uint32_t numer, uint32_t denom);

  /**
   * Selects one entry randomly from a given set of choices.  Each choice
   * has a certain "weight", and its probability is weight / total weights.
   * The parameter is the array of weights, and returned is an index into that
   * array that matches the selected outcome.  The sum of all weights must be
   * representable in an uint32.
   */
  size_t SelectByWeight (const std::vector<uint32_t>& weights);

};

} // namespace xaya

#endif // XAYAUTIL_RANDOM_HPP
