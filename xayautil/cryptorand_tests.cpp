// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cryptorand.hpp"

#include <gtest/gtest.h>

#include <glog/logging.h>

#include <set>

namespace xaya
{
namespace
{

TEST (CryptoRandTests, Uint256)
{
  /* This test obviously cannot verify that the returned bytes are truly
     random.  It just makes sure the function actually "works" (e.g. does not
     crash) and does not have obvious errors.  */

  constexpr unsigned generators = 10;
  constexpr unsigned tries = 1000;

  std::set<uint256> found;

  for (unsigned i = 0; i < generators; ++i)
    {
      CryptoRand rnd;
      for (unsigned j = 0; j < tries; ++j)
        {
          const auto num = rnd.Get<uint256> ();
          if (i == 0 && j == 0)
            LOG (INFO) << "First random number: " << num.ToHex ();
          ASSERT_FALSE (num.IsNull ());
          found.insert (num);
        }
    }

  ASSERT_EQ (found.size (), generators * tries);
}

} // anonymous namespace
} // namespace xaya
