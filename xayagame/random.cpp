// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random.hpp"

#include "hash.hpp"

#include <glog/logging.h>

#include <cstdint>

namespace xaya
{

Random::Random ()
{
  seed.SetNull ();
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

} // namespace xaya
