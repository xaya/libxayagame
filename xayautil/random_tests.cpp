// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random.hpp"

#include <gtest/gtest.h>

#include <glog/logging.h>

#include <limits>

namespace xaya
{
namespace
{

/** The seed (as hex string) that we use in tests.  */
constexpr const char SEED[]
    = "7ca22c1665349f6c2cf40c7f7923e18184bbf3baa2b4096bee511b7a7eaf87e8";

/**
 * The test fixture class for Random seeds the instance with a predetermined
 * seed.  We then also expect that the resulting random numbers match exactly
 * given golden values.  This is so that we notice any changes that affect
 * the generation of random numbers, as those changes would be consensus
 * critical for any games relying on Random.
 */
class RandomTests : public testing::Test
{

protected:

  Random rnd;

  RandomTests ()
  {
    uint256 seed;
    CHECK (seed.FromHex (SEED));
    rnd.Seed (seed);
  }

};

TEST_F (RandomTests, Bytes)
{
  const uint8_t expectedBytes[] = {
    /* This is the seed itself.  */
    0x7c, 0xa2, 0x2c, 0x16, 0x65, 0x34, 0x9f, 0x6c,
    0x2c, 0xf4, 0x0c, 0x7f, 0x79, 0x23, 0xe1, 0x81,
    0x84, 0xbb, 0xf3, 0xba, 0xa2, 0xb4, 0x09, 0x6b,
    0xee, 0x51, 0x1b, 0x7a, 0x7e, 0xaf, 0x87, 0xe8,

    /* Some following bytes based on correct "re-seeding".  */
    0x67, 0xd8, 0x11, 0xd6, 0x7f, 0xfb, 0x76, 0x45,
  };

  for (const auto b : expectedBytes)
    ASSERT_EQ (rnd.Next<uint8_t> (), b);
}

TEST_F (RandomTests, Bits)
{
  ASSERT_EQ (rnd.Next<bool> (), false);
  ASSERT_EQ (rnd.Next<bool> (), false);
  ASSERT_EQ (rnd.Next<bool> (), false);
  ASSERT_EQ (rnd.Next<bool> (), false);
  ASSERT_EQ (rnd.Next<bool> (), true);
  ASSERT_EQ (rnd.Next<bool> (), false);
  ASSERT_EQ (rnd.Next<bool> (), true);
  ASSERT_EQ (rnd.Next<bool> (), false);
}

TEST_F (RandomTests, Integers)
{
  ASSERT_EQ (rnd.Next<uint16_t> (), 0x7ca2);
  ASSERT_EQ (rnd.Next<uint32_t> (), 0x2c166534);
  ASSERT_EQ (rnd.Next<uint64_t> (), 0x9f6c2cf40c7f7923);
}

TEST_F (RandomTests, NextInt)
{
  constexpr unsigned n = 10;
  constexpr unsigned rolls = 10'000;
  constexpr unsigned threshold = rolls / n * 80 / 100;

  std::vector<unsigned> cnt(n);
  for (unsigned i = 0; i < rolls; ++i)
    ++cnt[rnd.NextInt (n)];

  for (unsigned i = 0; i < n; ++i)
    {
      LOG (INFO) << "Count for " << i << ": " << cnt[i];
      EXPECT_GE (cnt[i], threshold);
    }
}

TEST_F (RandomTests, NextIntLargeN)
{
  constexpr uint32_t n = std::numeric_limits<uint32_t>::max ();
  constexpr unsigned rolls = 1'000;
  constexpr uint32_t threshold = 4'000'000'000;

  for (unsigned i = 0; i < rolls; ++i)
    if (rnd.NextInt (n) >= threshold)
      return;

  FAIL () << "Threshold has never been exceeded";
}

TEST_F (RandomTests, ProbabilityRoll)
{
  constexpr uint32_t numer = 70;
  constexpr uint32_t denom = 100;
  constexpr unsigned rolls = 1'000'000;

  unsigned success = 0;
  for (unsigned i = 0; i < rolls; ++i)
    if (rnd.ProbabilityRoll (numer, denom))
      ++success;

  LOG (INFO)
      << "Rolled " << rolls << " tries for probability "
      << numer << "/" << denom << " and got " << success << " successes";
  EXPECT_GE (success, 690'000);
  EXPECT_LE (success, 710'000);
}

TEST_F (RandomTests, SelectByWeight)
{
  const std::vector<uint32_t> weights = {55, 10, 35};
  constexpr unsigned rolls = 1'000'000;

  unsigned counts[] = {0, 0, 0};
  for (unsigned i = 0; i < rolls; ++i)
    ++counts[rnd.SelectByWeight (weights)];

  for (unsigned i = 0; i < weights.size (); ++i)
    LOG (INFO)
        << "Choice " << i << " with weight " << weights[i]
        << " was selected " << counts[i] << " times";

  for (unsigned i = 0; i < weights.size (); ++i)
    {
      EXPECT_GE (counts[i], 10'000 * (weights[i] - 1));
      EXPECT_LE (counts[i], 10'000 * (weights[i] + 1));
    }
}

} // anonymous namespace
} // namespace xaya
