/*
 * WASM shim for xayautil/base64.cpp.
 * Replaces the OpenSSL EVP base64 backend with a standalone
 * implementation for use in the Emscripten WASM environment.
 *
 * Implements the same xaya::EncodeBase64 / xaya::DecodeBase64
 * API defined in xayautil/base64.hpp.
 */

#include <xayautil/base64.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

namespace xaya
{

/* Standard base64 encoding alphabet. */
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Inverse lookup table: maps ASCII characters to their 6-bit values.
   -1 indicates an invalid character. */
static const int b64_inv[256] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
  52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
  -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

std::string
EncodeBase64 (const std::string& data)
{
  std::string result;
  const size_t len = data.size ();
  result.reserve (4 * ((len + 2) / 3));

  const auto* in = reinterpret_cast<const unsigned char*> (data.data ());

  /* Process input in 3-byte groups, producing 4 base64 characters each. */
  for (size_t i = 0; i < len; i += 3)
    {
      uint32_t n = static_cast<uint32_t> (in[i]) << 16;
      if (i + 1 < len) n |= static_cast<uint32_t> (in[i + 1]) << 8;
      if (i + 2 < len) n |= static_cast<uint32_t> (in[i + 2]);

      result += b64_table[(n >> 18) & 0x3f];
      result += b64_table[(n >> 12) & 0x3f];
      result += (i + 1 < len) ? b64_table[(n >> 6) & 0x3f] : '=';
      result += (i + 2 < len) ? b64_table[n & 0x3f] : '=';
    }

  return result;
}

bool
DecodeBase64 (const std::string& encoded, std::string& data)
{
  /* Base64-encoded data must be a multiple of 4 characters. */
  if (encoded.size () % 4 != 0)
    return false;

  /* Count and validate padding characters ('='). */
  size_t padding = 0;
  for (const char c : encoded)
    {
      if (c != '=')
        {
          if (padding > 0) return false;  /* Padding in the middle. */
          continue;
        }
      ++padding;
    }
  if (padding >= 4)
    return false;

  data.clear ();
  data.reserve (encoded.size () / 4 * 3);

  /* Decode each 4-character group back to 3 bytes. */
  for (size_t i = 0; i < encoded.size (); i += 4)
    {
      int vals[4];
      for (int j = 0; j < 4; ++j)
        {
          unsigned char c = static_cast<unsigned char> (encoded[i + j]);
          if (c == '=')
            vals[j] = 0;
          else
            {
              vals[j] = b64_inv[c];
              if (vals[j] < 0)
                return false;
            }
        }

      uint32_t n = (vals[0] << 18) | (vals[1] << 12) | (vals[2] << 6) | vals[3];
      data += static_cast<char> ((n >> 16) & 0xff);
      if (encoded[i + 2] != '=')
        data += static_cast<char> ((n >> 8) & 0xff);
      if (encoded[i + 3] != '=')
        data += static_cast<char> (n & 0xff);
    }

  return true;
}

} // namespace xaya
