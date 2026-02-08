/*
 * WASM shim for xayautil/cryptorand.cpp.
 * Replaces OpenSSL's RAND_bytes with Emscripten's getentropy(),
 * which maps to the browser's crypto.getRandomValues() at runtime.
 *
 * Implements the same xaya::CryptoRand::Get<uint256>() API
 * defined in xayautil/cryptorand.hpp.
 */

#include <xayautil/cryptorand.hpp>

#include <unistd.h>  /* getentropy() - provided by Emscripten */

namespace xaya
{

template <>
uint256
CryptoRand::Get<uint256> ()
{
  unsigned char buf[uint256::NUM_BYTES];
  getentropy (buf, sizeof (buf));

  uint256 res;
  res.FromBlob (buf);
  return res;
}

} // namespace xaya
