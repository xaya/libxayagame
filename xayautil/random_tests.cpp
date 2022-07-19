// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glog/logging.h>

#include <limits>
#include <map>
#include <sstream>
#include <vector>

namespace xaya
{
namespace
{

using testing::ElementsAre;

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

TEST_F (RandomTests, Moving)
{
  ASSERT_EQ (rnd.Next<uint32_t> (), 0x7ca22c16);
  Random other = std::move (rnd);
  EXPECT_EQ (other.Next<uint32_t> (), 0x65349f6c);
  EXPECT_DEATH (rnd.Next<unsigned char> (), "has not been seeded");
}

TEST_F (RandomTests, BranchingOff)
{
  /* Branch off two different instances and check the expected values.
     Branching off again with the same key should yield the same bytes.  */
  Random branched = rnd.BranchOff ("foo");
  EXPECT_EQ (branched.Next<uint32_t> (), 0x34b115e1);
  branched = rnd.BranchOff ("bar");
  EXPECT_EQ (branched.Next<uint32_t> (), 0x3fc15d5e);
  branched = rnd.BranchOff ("foo");
  EXPECT_EQ (branched.Next<uint32_t> (), 0x34b115e1);

  /* All of this branching should not have altered the state of the
     instance itself.  */
  ASSERT_EQ (rnd.Next<uint32_t> (), 0x7ca22c16);

  /* If we now branch off, the modified next-byte index should result
     in a different sequence of bytes.  */
  branched = rnd.BranchOff ("foo");
  EXPECT_EQ (branched.Next<uint32_t> (), 0x1f49504c);

  /* Also if we let the initial Random reseed we should get (yet) another
     sequence of branched off bytes.  */
  for (unsigned i = 0; i < 3; ++i)
    rnd.Next<uint64_t> ();
  ASSERT_EQ (rnd.Next<uint32_t> (), 0x7eaf87e8);
  branched = rnd.BranchOff ("foo");
  EXPECT_EQ (branched.Next<uint32_t> (), 0x5a111de7);

  /* Verify the state of the initial Random again.  */
  ASSERT_EQ (rnd.Next<uint32_t> (), 0x67d811d6);
}

class ShuffleTests : public RandomTests
{

protected:

  /**
   * Shuffles a given vector of ints.
   */
  std::vector<int>
  Shuffle (std::vector<int> vec)
  {
    rnd.Shuffle (vec.begin (), vec.end ());
    return vec;
  }

  std::vector<int>
  ShuffleN (std::vector<int> vec, const size_t n)
  {
    rnd.ShuffleN (vec.begin (), vec.end (), n);
    return vec;
  }

};

TEST_F (ShuffleTests, Basic)
{
  /* Make sure that shuffling an empty or one-element array won't change
     the state of the random instance.  */
  EXPECT_THAT (Shuffle ({}), ElementsAre ());
  EXPECT_THAT (Shuffle ({42}), ElementsAre (42));
  EXPECT_EQ (rnd.Next<uint32_t> (), 0x7ca22c16);

  /* Do a proper shuffle and compare to expected "golden" data to ensure
     we are not accidentally changing the algorithm.  */
  EXPECT_THAT (Shuffle ({-5, 10, 0, 1'024, 20}),
               ElementsAre (0, 1'024, 20, 10, -5));
}

TEST_F (ShuffleTests, AllPermutationsPossible)
{
  const std::vector<int> input = {0, 1, 2, 3, 4};
  constexpr unsigned trials = 1'000'000;
  constexpr unsigned factorial = 1 * 2 * 3 * 4 * 5;
  constexpr unsigned threshold = 95 * trials / factorial / 100;

  std::map<std::vector<int>, int> found;
  for (unsigned i = 0; i < trials; ++i)
    ++found[Shuffle (input)];

  EXPECT_EQ (found.size (), factorial);
  for (const auto& entry : found)
    EXPECT_GE (entry.second, threshold);
}

TEST_F (ShuffleTests, DegenerateShuffleN)
{
  EXPECT_THAT (ShuffleN ({}, 10), ElementsAre ());
  EXPECT_THAT (ShuffleN ({42}, 1), ElementsAre (42));
  EXPECT_THAT (ShuffleN ({1, 2, 3, 4, 5}, 0), ElementsAre (1, 2, 3, 4, 5));
}

class SelectSubsetTests : public RandomTests
{

protected:

  /**
   * Computes and returns a subset of the range [0, n) consisting of
   * m elements.  This is based on ShuffleN.
   */
  std::set<int>
  SelectSubset (const int m, const int n)
  {
    std::vector<int> range;
    for (int i = 0; i < n; ++i)
      range.push_back (i);

    rnd.ShuffleN (range.begin (), range.end (), m);

    std::set<int> res;
    for (int i = 0; i < m; ++i)
      res.insert (range[i]);

    return res;
  }

};

TEST_F (SelectSubsetTests, Degenerate)
{
  EXPECT_THAT (SelectSubset (0, 0), ElementsAre ());
  EXPECT_THAT (SelectSubset (0, 1), ElementsAre ());
  EXPECT_THAT (SelectSubset (0, 1'000), ElementsAre ());

  EXPECT_THAT (SelectSubset (1, 1), ElementsAre (0));
  EXPECT_THAT (SelectSubset (5, 5), ElementsAre (0, 1, 2, 3, 4));
}

TEST_F (SelectSubsetTests, GoldenData)
{
  /* Ensure that we have not accidentally changed the algorithm.  */
  EXPECT_THAT (SelectSubset (5, 100), ElementsAre (9, 45, 71, 92, 95));
}

TEST_F (SelectSubsetTests, AllSubsetsPossible)
{
  /* We select 3-out-of-7, and ensure that we get all possible combinations.  */
  constexpr unsigned trials = 100'000;
  constexpr unsigned possible = (7 * 6 * 5) / (1 * 2 * 3);
  constexpr unsigned threshold = 95 * trials / possible / 100;

  std::map<std::string, unsigned> found;
  std::map<unsigned, unsigned> perNumber;
  for (unsigned i = 0; i < trials; ++i)
    {
      const auto cur = SelectSubset (3, 7);
      std::ostringstream str;
      for (const auto x : cur)
        {
          ++perNumber[x];
          str << x << " ";
        }
      ++found[str.str ()];
    }

  ASSERT_EQ (found.size (), possible);
  for (const auto& entry : found)
    EXPECT_GE (entry.second, threshold);

  ASSERT_EQ (perNumber.size (), 7);
  for (unsigned i = 0; i < 7; ++i)
    {
      EXPECT_GT (perNumber[i], 0);
      LOG (INFO) << "Number " << i << ": " << perNumber[i];
    }
}

} // anonymous namespace
} // namespace xaya
