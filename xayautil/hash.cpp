// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.hpp"

#include <glog/logging.h>

#include <openssl/sha.h>

#include <cstddef>

namespace xaya
{

/**
 * The actual implementation for the SHA256 state.  This is just a wrapper
 * around OpenSSL's SHA256_CTX.
 */
class SHA256::State
{

private:

  /** The underlying OpenSSL SHA256 state.  */
  SHA256_CTX ctx;

public:

  State ();

  State (const State&) = delete;
  void operator= (const State&) = delete;

  void Update (const unsigned char* data, size_t len);
  void Finalise (unsigned char* out);

};

SHA256::State::State ()
{
  SHA256_Init (&ctx);
}

void
SHA256::State::Update (const unsigned char* data, const size_t len)
{
  SHA256_Update (&ctx, data, len);
}

void
SHA256::State::Finalise (unsigned char* out)
{
  SHA256_Final (out, &ctx);
}

SHA256::SHA256 ()
{
  state = std::make_unique<State> ();
}

SHA256::~SHA256 () = default;

SHA256&
SHA256::operator<< (const std::string& data)
{
  CHECK (state != nullptr);
  state->Update (reinterpret_cast<const unsigned char*> (&data[0]),
                 data.size ());
  return *this;
}

SHA256&
SHA256::operator<< (const uint256& data)
{
  CHECK (state != nullptr);
  state->Update (data.GetBlob (), uint256::NUM_BYTES);
  return *this;
}

uint256
SHA256::Finalise ()
{
  static_assert (SHA256_DIGEST_LENGTH == uint256::NUM_BYTES,
                 "uint256 is not a valid output for SHA-256");

  CHECK (state != nullptr);

  unsigned char data[SHA256_DIGEST_LENGTH];
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
