/*
 * WASM shim for xayautil/hash.cpp.
 * Replaces the OpenSSL EVP SHA-256 backend with the standalone
 * sha256 implementation (sha256.h / sha256.cpp) for use in the
 * Emscripten WASM environment.
 *
 * Implements the same xaya::SHA256 public API defined in
 * xayautil/hash.hpp, so all existing code using SHA256 works
 * without modification.
 */

#include <xayautil/hash.hpp>
#include "sha256.h"

#include <cstddef>
#include <cassert>

namespace xaya
{

/* SHA256::State wraps the standalone sha256_ctx instead of OpenSSL. */
class SHA256::State
{
private:
  sha256_ctx ctx;

public:
  State () { sha256_init (&ctx); }

  State (const State&) = delete;
  void operator= (const State&) = delete;

  void Update (const unsigned char* data, size_t len)
  {
    sha256_update (&ctx, data, len);
  }

  void Finalise (unsigned char* out)
  {
    sha256_final (&ctx, out);
  }
};

SHA256::SHA256 ()
{
  state = std::make_unique<State> ();
}

SHA256::~SHA256 () = default;

SHA256&
SHA256::operator<< (const std::string& data)
{
  assert (state != nullptr);
  state->Update (reinterpret_cast<const unsigned char*> (&data[0]),
                 data.size ());
  return *this;
}

SHA256&
SHA256::operator<< (const uint256& data)
{
  assert (state != nullptr);
  state->Update (data.GetBlob (), uint256::NUM_BYTES);
  return *this;
}

uint256
SHA256::Finalise ()
{
  assert (state != nullptr);

  unsigned char data[32];
  state->Finalise (data);

  uint256 res;
  res.FromBlob (data);

  return res;
}

uint256
SHA256::Hash (const std::string& data)
{
  SHA256 hasher;
  hasher << data;
  return hasher.Finalise ();
}

} // namespace xaya
