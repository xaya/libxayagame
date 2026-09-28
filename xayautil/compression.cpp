// Copyright (C) 2019-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compression_internal.hpp"

#include "base64.hpp"

#include <map>

namespace xaya
{

/* ************************************************************************** */

namespace
{

/** Maximum window size in bits used for the consensus-compression.  */
constexpr int WINDOW_BITS = 15;

/** Compression level we use.  */
constexpr int LEVEL = Z_BEST_COMPRESSION;

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
        CHECK_LE (stream.total_in, input.size ());
        if (stream.total_in < input.size ())
          {
            VLOG (1)
                << "Trailing bytes in compressed data: "
                << stream.total_in << " consumed of " << input.size ();
            return false;
          }
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

/**
 * Serialises JSON in the way we use for CompressJson.
 */
std::string
SerialiseJsonForCompression (const Json::Value& val)
{
  Json::StreamWriterBuilder wbuilder;
  wbuilder["commentStyle"] = "None";
  wbuilder["indentation"] = "";
  wbuilder["enableYAMLCompatibility"] = false;
  wbuilder["dropNullPlaceholders"] = false;
  wbuilder["useSpecialFloats"] = false;
  wbuilder["emitUTF8"] = false;

  return Json::writeString (wbuilder, val);
}

/**
 * Returns true if all bytes in the given string are ASCII, i.e. none of
 * them has the highest bit set.  Note that our canonical serialisation
 * (with emitUTF8 disabled) is always pure ASCII, so this is a necessary
 * property for any accepted input.
 */
bool
IsPrintableASCII (const std::string& s)
{
  for (const char c : s)
    {
      const unsigned char u = static_cast<unsigned char> (c);
      if (u < 0x20 || u >= 0x80)
        return false;
    }

  return true;
}

/**
 * Skips over a JSON string token starting at the given position, advancing
 * the position past the closing quote.  This assumes that the token is a
 * valid JSON string (which is guaranteed if the input was accepted by our
 * parser before).  Returns false if no well-formed token is found.
 */
bool
SkipJsonString (const std::string& s, size_t& pos)
{
  if (pos >= s.size () || s[pos] != '"')
    return false;
  ++pos;

  while (pos < s.size ())
    {
      const char c = s[pos];
      if (c == '\\')
        {
          if (pos + 1 >= s.size ())
            return false;
          pos += 2;
        }
      else if (c == '"')
        {
          ++pos;
          return true;
        }
      else
        ++pos;
    }

  return false;
}

/**
 * Checks that the JSON text starting at the given position is exactly the
 * canonical serialisation of the given value (as done by CompressJson),
 * with the sole exception that object members may appear in any order.
 * On success, the position is advanced past the parsed value and true is
 * returned.
 *
 * This can be used as a strict "re-encoding" check: it rejects any input
 * that is not byte-for-byte the canonical form, except for member ordering,
 * which is not significant in JSON.
 */
bool
CheckCanonicalJson (const std::string& s, size_t& pos, const Json::Value& val)
{
  if (val.isObject ())
    {
      if (pos >= s.size () || s[pos] != '{')
        return false;
      ++pos;

      /* Map the canonical serialisation of each member name to the name
         itself, so that we can look up raw key tokens as we encounter
         them (and enforce that the key escaping is canonical).  */
      std::map<std::string, std::string> members;
      for (const auto& name : val.getMemberNames ())
        members.emplace (SerialiseJsonForCompression (name), name);

      if (val.empty ())
        {
          if (pos >= s.size () || s[pos] != '}')
            return false;
          ++pos;
          return true;
        }

      while (true)
        {
          const size_t start = pos;
          if (!SkipJsonString (s, pos))
            return false;
          const std::string rawKey = s.substr (start, pos - start);

          const auto mit = members.find (rawKey);
          if (mit == members.end ())
            return false;
          const std::string name = mit->second;
          members.erase (mit);

          if (pos >= s.size () || s[pos] != ':')
            return false;
          ++pos;

          if (!CheckCanonicalJson (s, pos, val[name]))
            return false;

          if (pos < s.size () && s[pos] == ',')
            {
              ++pos;
              continue;
            }
          if (pos < s.size () && s[pos] == '}')
            {
              ++pos;
              break;
            }

          return false;
        }

      return members.empty ();
    }

  if (val.isArray ())
    {
      if (pos >= s.size () || s[pos] != '[')
        return false;
      ++pos;

      if (val.empty ())
        {
          if (pos >= s.size () || s[pos] != ']')
            return false;
          ++pos;
          return true;
        }

      for (Json::ArrayIndex i = 0; i < val.size (); ++i)
        {
          if (!CheckCanonicalJson (s, pos, val[i]))
            return false;

          if (i + 1 < val.size ())
            {
              if (pos >= s.size () || s[pos] != ',')
                return false;
              ++pos;
            }
        }

      if (pos >= s.size () || s[pos] != ']')
        return false;
      ++pos;
      return true;
    }

  /* Scalars must match the canonical serialisation exactly.  */
  const std::string canon = SerialiseJsonForCompression (val);
  if (pos > s.size () || s.compare (pos, canon.size (), canon) != 0)
    return false;
  pos += canon.size ();

  return true;
}

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
  if (!val.isObject () && !val.isArray ())
    {
      LOG (WARNING) << "CompressJson expects object or array: " << val;
      return false;
    }

  uncompressed = SerialiseJsonForCompression (val);
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
  rbuilder["allowTrailingCommas"] = false;
  rbuilder["stackLimit"] = stackLimit;
  rbuilder["failIfExtra"] = true;
  rbuilder["rejectDupKeys"] = true;
  rbuilder["skipBom"] = false;
  rbuilder["allowSpecialFloats"] = false;

  std::string compressed;
  if (!DecodeBase64 (input, compressed))
    return false;

  if (!UncompressData (compressed, maxOutputSize, uncompressed))
    return false;

  /* Our canonical serialisation is always pure ASCII, so reject any input
     containing raw non-ASCII bytes.  Non-ASCII characters have to be
     escaped as \uXXXX instead.  */
  if (!IsPrintableASCII (uncompressed))
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

  /* As an extra safety net, we require that the string serialises back
     byte-equal to the original input, except that object members may be
     ordered arbitrarily (which is not significant in JSON).  */
  size_t pos = 0;
  if (!CheckCanonicalJson (uncompressed, pos, output)
        || pos != uncompressed.size ())
    return false;

  return output.isObject () || output.isArray ();
}

/* ************************************************************************** */

} // namespace xaya
