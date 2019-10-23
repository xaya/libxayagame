// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random.hpp"

#include "hash.hpp"

#include <glog/logging.h>

#include <cstdint>
#include <limits>

namespace xaya
{

Random::Random ()
{
  seed.SetNull ();
}

Random::Random (Random&& other)
{
  *this = std::move (other);
}

Random&
Random::operator= (Random&& other)
{
  seed = std::move (other.seed);
  nextIndex = other.nextIndex;

  other.seed.SetNull ();

  return *this;
}

void
Random::Seed (const uint256& s)
{
  seed = s;
  nextIndex = 0;
}

template <>
  unsigned char
  Random::Next<unsigned char> ()
{
  CHECK (!seed.IsNull ()) << "Random instance has not been seeded";

  CHECK_LE (nextIndex, uint256::NUM_BYTES);
  if (nextIndex == uint256::NUM_BYTES)
    {
      SHA256 hasher;
      hasher << seed;
      seed = hasher.Finalise ();
      nextIndex = 0;
    }

  const unsigned char* data = seed.GetBlob ();
  return data[nextIndex++];
}

template <>
  bool
  Random::Next<bool> ()
{
  return Next<unsigned char> () & 1;
}

namespace
{

/**
 * Requests two "lower bit" integers from the Random instance and combines
 * them to one integer with double the number of bits.  The two numbers are
 * combined in a big-endian fashion.
 */
template <typename T, typename Half, unsigned HalfBits>
  T
  CombineHalfInts (Random& rnd)
{
  T res = rnd.Next<Half> ();
  res <<= HalfBits;
  res |= rnd.Next<Half> ();

  return res;
}

} // anonymous namespace

template <>
  uint16_t
  Random::Next<uint16_t> ()
{
  return CombineHalfInts<uint16_t, unsigned char, 8> (*this);
}

template <>
  uint32_t
  Random::Next<uint32_t> ()
{
  return CombineHalfInts<uint32_t, uint16_t, 16> (*this);
}

template <>
  uint64_t
  Random::Next<uint64_t> ()
{
  return CombineHalfInts<uint64_t, uint32_t, 32> (*this);
}

uint32_t
Random::NextInt (const uint32_t n)
{
  CHECK_GT (n, 0);

  /* If we just take a random uint64 x and return "x % n", then smaller numbers
     are (very slightly) more probable than larger ones.  But if we make sure
     that x is from a range [0, m) where m is a multiple of n, then all
     numbers are equally likely to occur from the mod.  We can achieve this
     by rerolling x if it is larger than m.  This is negligible probability of
     occuring, so it is not hard performance wise either.  */

  const uint64_t factor = std::numeric_limits<uint64_t>::max () / n;
  const uint64_t m = factor * n;

  while (true)
    {
      const uint64_t x = Next<uint64_t> ();
      if (x < m)
        return x % n;
    }
}

bool
Random::ProbabilityRoll (uint32_t numer, uint32_t denom)
{
  const auto val = NextInt (denom);
  return val < numer;
}

size_t
Random::SelectByWeight (const std::vector<uint32_t>& weights)
{
  uint64_t totalWeights = 0;
  for (const auto w : weights)
    totalWeights += w;
  CHECK_LE (totalWeights, std::numeric_limits<uint32_t>::max ());

  uint32_t roll = NextInt (totalWeights);
  for (size_t i = 0; i < weights.size (); ++i)
    {
      if (roll < weights[i])
        return i;
      roll -= weights[i];
    }

  LOG (FATAL) << "No option selected";
}

} // namespace xaya
