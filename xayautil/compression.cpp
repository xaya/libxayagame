// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compression_internal.hpp"

#include "base64.hpp"

namespace xaya
{

/* ************************************************************************** */

namespace
{

/** Maximum window size in bits used for the consensus-compression.  */
constexpr int WINDOW_BITS = 15;

/** Compression level we use.  */
constexpr int LEVEL = 9;

/**
 * Utility class wrapping a z_stream instance used for inflating data.
 */
class InflateStream : public BasicZlibStream
{

public:

  /**
   * Initialises the inflate struct with our parameters.
   */
  InflateStream ()
  {
    const auto res = inflateInit2 (&stream, -WINDOW_BITS);
    CHECK_EQ (res, Z_OK) << "Inflate init error " << res << ": " << GetError ();
  }

  /**
   * Frees the allocated stream state.
   */
  ~InflateStream ()
  {
    const auto res = inflateEnd (&stream);
    CHECK_EQ (res, Z_OK) << "Inflate end error " << res << ": " << GetError ();
  }

  /**
   * Performs the actual uncompression step of data.
   */
  bool
  Uncompress (const std::string& input, const size_t maxOutputSize,
              std::string& output)
  {
    SetInput (input);
    SetOutputSize (maxOutputSize);

    const auto res = inflate (&stream, Z_FINISH);
    switch (res)
      {
      case Z_STREAM_END:
        CHECK_EQ (stream.total_in, input.size ());
        output = ExtractOutput ();
        return true;

      case Z_BUF_ERROR:
        VLOG (1)
            << "Uncompress produced too much output data; processed "
            << stream.total_in << " input bytes of the total " << input.size ();
        return false;

      case Z_NEED_DICT:
      case Z_DATA_ERROR:
        VLOG (1) << "Invalid data provided to uncompress: " << GetError ();
        return false;

      default:
        LOG (FATAL) << "Inflate error " << res << ": " << GetError ();
      }
  }

};

} // anonymous namespace

std::string
CompressData (const std::string& data)
{
  DeflateStream compressor(-WINDOW_BITS, LEVEL);
  return compressor.Compress (data);
}

bool
UncompressData (const std::string& input, const size_t maxOutputSize,
                std::string& output)
{
  InflateStream uncompressor;
  return uncompressor.Uncompress (input, maxOutputSize, output);
}

/* ************************************************************************** */

bool
CompressJson (const Json::Value& val,
              std::string& encoded, std::string& uncompressed)
{
  Json::StreamWriterBuilder wbuilder;
  wbuilder["commentStyle"] = "None";
  wbuilder["indentation"] = "";
  wbuilder["enableYAMLCompatibility"] = false;
  wbuilder["dropNullPlaceholders"] = false;
  wbuilder["useSpecialFloats"] = false;

  if (!val.isObject () && !val.isArray ())
    {
      LOG (WARNING) << "CompressJson expects object or array: " << val;
      return false;
    }

  uncompressed = Json::writeString (wbuilder, val);
  encoded = EncodeBase64 (CompressData (uncompressed));

  return true;
}

bool
UncompressJson (const std::string& input,
                const size_t maxOutputSize, const unsigned stackLimit,
                Json::Value& output, std::string& uncompressed)
{
  Json::CharReaderBuilder rbuilder;
  rbuilder["allowComments"] = false;
  /* Without strictRoot, versions of jsoncpp before
     https://github.com/open-source-parsers/jsoncpp/pull/1014 did not
     properly enforce failIfExtra (which we want).  */
  rbuilder["strictRoot"] = true;
  rbuilder["allowDroppedNullPlaceholders"] = false;
  rbuilder["allowNumericKeys"] = false;
  rbuilder["allowSingleQuotes"] = false;
  rbuilder["stackLimit"] = stackLimit;
  rbuilder["failIfExtra"] = true;
  rbuilder["rejectDupKeys"] = true;
  rbuilder["allowSpecialFloats"] = false;

  std::string compressed;
  if (!DecodeBase64 (input, compressed))
    return false;

  if (!UncompressData (compressed, maxOutputSize, uncompressed))
    return false;

  std::string parseErrs;
  std::istringstream in(uncompressed);
  try
    {
      if (!Json::parseFromStream (rbuilder, in, &output, &parseErrs))
        return false;
    }
  catch (const Json::Exception& exc)
    {
      return false;
    }

  return output.isObject () || output.isArray ();
}

/* ************************************************************************** */

} // namespace xaya
