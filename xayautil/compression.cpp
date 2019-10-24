// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compression_internal.hpp"

namespace xaya
{

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

} // namespace xaya
