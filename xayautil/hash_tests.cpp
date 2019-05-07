// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.hpp"

#include <gtest/gtest.h>

namespace xaya
{
namespace
{

class SHA256Tests : public testing::Test
{

protected:

  SHA256 hasher;

};

TEST_F (SHA256Tests, Empty)
{
  EXPECT_EQ (hasher.Finalise ().ToHex (),
     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F (SHA256Tests, NonEmpty)
{
  uint256 someData;
  ASSERT_TRUE (someData.FromHex (
      "2e773fdbfcb9e80875ce3f2f44a4d17fd9d6a62023cad54bc79f394403e6a6ab"));

  hasher << "foo";
  hasher << "";
  hasher << someData;
  hasher << "";
  hasher << "bar";

  /* Total data that is being hashed (in hex):
      666f6f
      2e773fdbfcb9e80875ce3f2f44a4d17fd9d6a62023cad54bc79f394403e6a6ab
      626172
  */
  EXPECT_EQ (hasher.Finalise ().ToHex (),
      "bdd7344649494d3f16b5c3bbc9989efe64bba2ce0651d6980aab2f12cef4fb0d");
}

TEST_F (SHA256Tests, UtilityHash)
{
  EXPECT_EQ (SHA256::Hash ("").ToHex (),
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ (SHA256::Hash ("foobar").ToHex (),
      "c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2");
}

} // anonymous namespace
} // namespace xaya
