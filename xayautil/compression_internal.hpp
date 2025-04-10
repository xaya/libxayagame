// Copyright (C) 2019-2025 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* This file contains internal implementation details for compression.cpp,
   but so that they can be shared also with the testing logic.  */

#ifndef XAYAUTIL_COMPRESSION_INTERNAL_HPP
#define XAYAUTIL_COMPRESSION_INTERNAL_HPP

#include "compression.hpp"

#include <glog/logging.h>

#include <zlib.h>

#include <algorithm>
#include <memory>

namespace xaya
{

namespace
{

/** Memory-usage level we use.  */
constexpr int MEM_LEVEL = 9;

/**
 * Basic wrapper class around a z_stream.  It does not do anything except
 * set up the allocator functions to Z_NULL in the constructor.
 */
class BasicZlibStream
{

private:

  /**
   * Buffer for the output data.  We allocate it up to the maximum size,
   * which might be a lot bigger than what we really need.  Thus we avoid
   * using std::string here, which would unnecessarily fill the buffer up
   * with zeros of that larger length.
   */
  std::unique_ptr<char[]> output;

  /** Size of the allocated buffer.  */
  size_t outputSize;

protected:

  /** The underlying zlib stream struct.  */
  z_stream stream;

  BasicZlibStream ()
  {
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
  }

  /**
   * Returns the last error message from the stream, handling NULL (no error)
   * correctly.
   */
  std::string
  GetError () const
  {
    if (stream.msg == nullptr)
      return "<none>";
    return stream.msg;
  }

  /**
   * Fills in the input in the stream struct to reference the memory
   * (and length) of the given byte string.
   */
  void
  SetInput (const std::string& input)
  {
    const Bytef* inputBuf = reinterpret_cast<const Bytef*> (input.data ());
    stream.next_in = const_cast<Bytef*> (inputBuf);
    stream.avail_in = input.size ();
  }

  /**
   * Sets the size of the output buffer, and points the stream to it.  If output
   * is already written, it will be preserved (so the buffer can be enlarged
   * as needed).
   */
  void
  SetOutputSize (const size_t sz)
  {
    const size_t alreadyWritten = stream.total_out;
    CHECK_GE (sz, alreadyWritten)
        << "Can't shrink output buffer below what is already written";

    std::unique_ptr<char[]> newOutput(new char[sz]);
    std::copy (output.get (), output.get () + alreadyWritten, newOutput.get ());

    outputSize = sz;
    output = std::move (newOutput);

    stream.next_out = reinterpret_cast<Bytef*> (output.get () + alreadyWritten);
    stream.avail_out = outputSize - alreadyWritten;
  }

  /**
   * Returns the output with correct length (as per the stream's total_out)
   * as a string.
   */
  std::string
  ExtractOutput () const
  {
    CHECK_LE (stream.total_out, outputSize);
    return std::string (output.get (), stream.total_out);
  }

};

/**
 * Utility class wrapping z_stream for deflation.  This handles initialisation
 * and destruction of the stream struct (and its associated data) through RAII.
 */
class DeflateStream : public BasicZlibStream
{

public:

  /**
   * Initialises the deflate struct with some default parameters and the
   * given other parameters.  The custom parameters are used to construct
   * test data in the unit test.  The main implementation always sets them
   * to specific constants.
   */
  explicit DeflateStream (const int windowBits, const int level)
  {
    const auto res = deflateInit2 (&stream, level, Z_DEFLATED, windowBits,
                                   MEM_LEVEL, Z_DEFAULT_STRATEGY);
    CHECK_EQ (res, Z_OK) << "Deflate init error " << res << ": " << GetError ();
  }

  /**
   * Frees the allocated stream state.
   */
  ~DeflateStream ()
  {
    const auto res = deflateEnd (&stream);
    CHECK_EQ (res, Z_OK) << "Deflate end error " << res << ": " << GetError ();
  }

  /**
   * Sets a dictionary for the compression.  This is only used for testing,
   * in order to construct data that requires a dictionary for inflation.
   */
  void
  SetDictionary (const std::string& dict)
  {
    LOG (WARNING)
        << "Setting a dictionary for compression, this data will not"
           " be accepted by the consensus uncompress function";
    const Bytef* data = reinterpret_cast<const Bytef*> (dict.data ());
    const auto res = deflateSetDictionary (&stream, data, dict.size ());
    CHECK_EQ (res, Z_OK)
        << "Set dictionary error " << res << ": " << GetError ();
  }

  /**
   * Performs the actual compression of input data, using our stream.
   * This function must be called only once on the instance.
   */
  std::string
  Compress (const std::string& data)
  {
    uInt dictLength;
    auto res = deflateGetDictionary (&stream, Z_NULL, &dictLength);
    CHECK_EQ (res, Z_OK)
        << "Get dictionary length error " << res << ": " << GetError ();

    const size_t outputBound = deflateBound (&stream, data.size ());
    SetInput (data);
    SetOutputSize (outputBound);

    /* There is a bug in deflateBound in some versions of zlib, which causes it
       to return a value too small: https://github.com/madler/zlib/issues/758

       For this reason, we implement a work around.  Even though deflate()
       in our situation "should" be a one-shot, we handle the case that the
       output buffer runs out and stock it up until deflate() finishes.  */

    while (true)
      {
        res = deflate (&stream, Z_FINISH);
        if (res == Z_STREAM_END)
          break;

        CHECK (res == Z_OK || res == Z_BUF_ERROR)
            << "Deflate error " << res << ": " << GetError ();

        LOG (WARNING)
            << "Increasing deflate output buffer by " << outputBound
            << " due to bug in deflateBound";
        SetOutputSize (stream.total_out + outputBound);
      }

    CHECK_EQ (stream.total_in, dictLength + data.size ());

    const std::string output = ExtractOutput ();
    VLOG (2) << "Compressed " << data.size () << " bytes to " << output.size ();

    return output;
  }

};

} // anonymous namespace

} // namespace xaya

#endif // XAYAUTIL_COMPRESSION_INTERNAL_HPP
