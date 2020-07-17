// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compression_internal.hpp"

#include "base64.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <vector>

namespace xaya
{
namespace
{

/* ************************************************************************** */

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

/* ************************************************************************** */

class JsonCompressionTests : public testing::Test
{

protected:

  /**
   * Parses a given string into JSON.
   */
  static Json::Value
  ParseJson (const std::string& str)
  {
    std::istringstream in(str);
    Json::Value res;
    in >> res;
    return res;
  }

};

TEST_F (JsonCompressionTests, Roundtrip)
{
  const std::string tests[] =
    {
      "{}",
      "[]",
      "[1, 2, 3]",
      R"({
        "foo":
          {
            "bar": 10,
            "x": true,
            "y": null,
            "z": [1, "", 5]
          },
        "z": 1.5
      })",
    };

  for (const auto& t : tests)
    {
      const auto input = ParseJson (t);

      std::string encoded;
      std::string uncompressed;
      ASSERT_TRUE (CompressJson (input, encoded, uncompressed));
      EXPECT_EQ (ParseJson (uncompressed), input);

      Json::Value output = "dummy is overwritten";
      std::string uncompressed2;
      ASSERT_TRUE (UncompressJson (encoded, 100, 10, output, uncompressed2));
      EXPECT_EQ (output, input);
      EXPECT_EQ (uncompressed2, uncompressed);
    }
}

TEST_F (JsonCompressionTests, SerialisedJsonFormat)
{
  const auto input = ParseJson (R"(
    {
      "foo": "bar",
      "baz": null
    }
  )");
  const std::string expectedString = R"({"baz":null,"foo":"bar"})";

  std::string encoded;
  std::string uncompressed;
  ASSERT_TRUE (CompressJson (input, encoded, uncompressed));
  EXPECT_EQ (uncompressed, expectedString);
}

TEST_F (JsonCompressionTests, NotObjectOrArray)
{
  std::string encoded;
  std::string uncompressed;
  EXPECT_FALSE (CompressJson (ParseJson ("null"), encoded, uncompressed));
  EXPECT_FALSE (CompressJson (ParseJson ("42"), encoded, uncompressed));
  EXPECT_FALSE (CompressJson (ParseJson ("\"foobar\""), encoded, uncompressed));
}

TEST_F (JsonCompressionTests, MaxOutputSize)
{
  const auto input = ParseJson (R"(["foobar"])");

  std::string encoded;
  std::string uncompressed;
  ASSERT_TRUE (CompressJson (input, encoded, uncompressed));

  Json::Value output;
  std::string uncompressed2;
  EXPECT_FALSE (UncompressJson (encoded, uncompressed.size () - 1, 10,
                                output, uncompressed2));
  ASSERT_TRUE (UncompressJson (encoded, uncompressed.size (), 10,
                               output, uncompressed2));
  EXPECT_EQ (output, input);
}

TEST_F (JsonCompressionTests, StackLimit)
{
  const auto input = ParseJson (R"([[[[{}]]]])");

  std::string encoded;
  std::string uncompressed;
  ASSERT_TRUE (CompressJson (input, encoded, uncompressed));

  Json::Value output;
  std::string uncompressed2;
  EXPECT_FALSE (UncompressJson (encoded, 100, 4, output, uncompressed2));
  ASSERT_TRUE (UncompressJson (encoded, 100, 5, output, uncompressed2));
  EXPECT_EQ (output, input);
}

TEST_F (JsonCompressionTests, InvalidBase64)
{
  Json::Value output;
  std::string uncompressed2;
  EXPECT_FALSE (UncompressJson ("invalid base64", 100, 10,
                                output, uncompressed2));
}

TEST_F (JsonCompressionTests, InvalidCompressedData)
{
  const std::string encoded = EncodeBase64 ("invalid compressed data");

  Json::Value output;
  std::string uncompressed2;
  EXPECT_FALSE (UncompressJson (encoded, 100, 10, output, uncompressed2));
}

TEST_F (JsonCompressionTests, WhitespaceOk)
{
  const std::string serialised = R"(
    {
      "value": "with trailing whitespace"
    }
  )";

  const std::string encoded = EncodeBase64 (CompressData (serialised));

  Json::Value output;
  std::string uncompressed2;
  ASSERT_TRUE (UncompressJson (encoded, 100, 10, output, uncompressed2));
  EXPECT_EQ (output, ParseJson (serialised));
  EXPECT_EQ (uncompressed2, serialised);
}

TEST_F (JsonCompressionTests, InvalidSerialisedJson)
{
  const std::string tests[] =
    {
      "",
      "\"string\"",
      "42",
      "null",
      "true",
      "{1: 2}",
      R"({"foo": NaN})",
      R"({"foo": 0, "foo": 1})",
      R"({"foo": 'single quotes'})",
      "junk {}",
      "{} junk",
      "{} 42",
    };

  for (const auto& t : tests)
    {
      const std::string encoded = EncodeBase64 (CompressData (t));

      Json::Value output;
      std::string uncompressed2;
      EXPECT_FALSE (UncompressJson (encoded, 100, 10, output, uncompressed2));
    }
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
