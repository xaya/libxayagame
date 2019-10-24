// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compression_internal.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace xaya
{
namespace
{

class CompressionTests : public testing::Test
{

protected:

  /**
   * Uncompresses data and expects it to be valid, matching the given
   * original input string.
   */
  static void
  ExpectValidUncompress (const std::string& compressed, const size_t maxSize,
                         const std::string& expected)
  {
    std::string actual;
    ASSERT_TRUE (UncompressData (compressed, maxSize, actual));
    EXPECT_EQ (actual, expected);
  }

  /**
   * Tries to uncompress with the given data and expects it to fail.
   */
  static void
  ExpectInvalidUncompress (const std::string& compressed, const size_t maxSize)
  {
    std::string output;
    EXPECT_FALSE (UncompressData (compressed, maxSize, output));
  }

};

TEST_F (CompressionTests, RoundTrip)
{
  std::string longString;
  for (unsigned i = 0; i < 1'000'000; ++i)
    longString.append ("abcdef");

  const std::vector<std::string> tests =
    {
      "", "123", u8"äöü",
      R"({"tactics":{"actions":[{"foo":10},{"bar":42}]}})",
      std::string ("foo\0bar", 7),
      longString,
    };

  for (const auto& str : tests)
    {
      const std::string compressed = CompressData (str);
      ExpectValidUncompress (compressed, str.size (), str);
    }
}

TEST_F (CompressionTests, MaxOutputSize)
{
  const std::string input = "foobar";
  const std::string compressed = CompressData (input);

  ExpectValidUncompress (compressed, input.size (), input);
  ExpectValidUncompress (compressed, 1'000'000, input);
  ExpectInvalidUncompress (compressed, input.size () - 1);
}

TEST_F (CompressionTests, InvalidData)
{
  ExpectInvalidUncompress ("not valid compressed data", 100);
}

TEST_F (CompressionTests, CompressionLevelZero)
{
  const std::string input = "foobar";

  DeflateStream compressor(-15, 0);
  const std::string compressed = compressor.Compress (input);

  ExpectValidUncompress (compressed, input.size (), input);
}

TEST_F (CompressionTests, SmallerWindowSize)
{
  const std::string input = "foobar";

  DeflateStream compressor(-10, 9);
  const std::string compressed = compressor.Compress (input);

  ExpectValidUncompress (compressed, input.size (), input);
}

/* It would be cool to test also that data encoded with a *larger* window size
   is rejected by UncompressData, but this is not easily possible as the chosen
   value of 15 is the largest that zlib allows anyway at the moment.  */

TEST_F (CompressionTests, WithDictionary)
{
  const std::string input = "123xyz foobar";

  DeflateStream compressor(-15, 9);
  compressor.SetDictionary ("foobar");
  const std::string compressed = compressor.Compress (input);

  ExpectInvalidUncompress (compressed, input.size ());
}

TEST_F (CompressionTests, ZlibFormat)
{
  const std::string input = "foobar";

  DeflateStream compressor(15, 9);
  const std::string compressed = compressor.Compress (input);

  ExpectInvalidUncompress (compressed, input.size ());
}

TEST_F (CompressionTests, GzipFormat)
{
  const std::string input = "foobar";

  DeflateStream compressor(15 + 16, 9);
  const std::string compressed = compressor.Compress (input);

  ExpectInvalidUncompress (compressed, input.size ());
}

} // anonymous namespace
} // namespace xaya
