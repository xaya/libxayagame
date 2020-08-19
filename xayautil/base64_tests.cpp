// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base64.hpp"

#include <gtest/gtest.h>

#include <string>

namespace xaya
{
namespace
{

using Base64Tests = testing::Test;

TEST_F (Base64Tests, Golden)
{
  struct TestCase
  {
    std::string data;
    std::string encoded;
  };
  const TestCase tests[] = {
    {"", ""},
    {"x", "eA=="},
    {"ab", "YWI="},
    {"z z", "eiB6"},
    {std::string ("\0\xFF\0\xFF", 4), "AP8A/w=="},
  };

  for (const auto& t : tests)
    {
      EXPECT_EQ (EncodeBase64 (t.data), t.encoded);
      std::string decoded;
      ASSERT_TRUE (DecodeBase64 (t.encoded, decoded));
      EXPECT_EQ (decoded, t.data);
    }
}

TEST_F (Base64Tests, DifferentLengths)
{
  for (int n = 0; n < 100; ++n)
    {
      const std::string data(n, 'x');
      const std::string encoded = EncodeBase64 (data);
      std::string decoded;
      ASSERT_TRUE (DecodeBase64 (encoded, decoded));
      EXPECT_EQ (decoded, data);
    }
}

TEST_F (Base64Tests, OutputAlphabet)
{
  std::ostringstream data;
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j <= 0xFF; ++j)
      data << static_cast<char> (j);
  ASSERT_EQ (data.str ().size (), 10 << 8);

  const std::string encoded = EncodeBase64 (data.str ());
  for (const auto c : encoded)
    EXPECT_TRUE (
          (c >= '0' && c <= '9')
       || (c >= 'A' && c <= 'Z')
       || (c >= 'a' && c <= 'z')
       || c == '+' || c == '/' || c == '=');

  std::string decoded;
  ASSERT_TRUE (DecodeBase64 (encoded, decoded));
  EXPECT_EQ (decoded, data.str ());
}

TEST_F (Base64Tests, InvalidDecode)
{
  for (const std::string s : {"xyz", "ab.=", "====", "AAAA====", "AA=A",
                              "AAA\n", "AAA=\n"})
    {
      std::string decoded;
      EXPECT_FALSE (DecodeBase64 (s, decoded))
          << "Decoded (should have been invalid): " << s << " to " << decoded;
    }
}

} // anonymous namespace
} // namespace xaya
