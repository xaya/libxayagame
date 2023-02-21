// Copyright (C) 2019-2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.hpp"

#include <glog/logging.h>

#include <openssl/evp.h>

#include <cstddef>

namespace xaya
{

namespace
{

constexpr size_t SHA256_DIGEST_LEN = 32;

} // anonymous namespace

/**
 * The actual implementation for the SHA256 state.  This is just a wrapper
 * around OpenSSL's SHA256_CTX.
 */
class SHA256::State
{

private:

  /** The digest to use (SHA-256).  */
  const EVP_MD* const digest;

  /** The underlying OpenSSL SHA256 state.  */
  EVP_MD_CTX* ctx;

public:

  State ();
  ~State ();

  State (const State&) = delete;
  void operator= (const State&) = delete;

  void Update (const unsigned char* data, size_t len);
  void Finalise (unsigned char* out);

};

SHA256::State::State ()
  : digest(EVP_sha256 ()), ctx(EVP_MD_CTX_new ())
{
  CHECK (digest != nullptr);
  CHECK (ctx != nullptr);
  CHECK_EQ (EVP_DigestInit_ex (ctx, digest, nullptr), 1);
}

SHA256::State::~State ()
{
  EVP_MD_CTX_free (ctx);
}

void
SHA256::State::Update (const unsigned char* data, const size_t len)
{
  CHECK_EQ (EVP_DigestUpdate (ctx, data, len), 1);
}

void
SHA256::State::Finalise (unsigned char* out)
{
  unsigned outlen;
  CHECK_EQ (EVP_DigestFinal_ex (ctx, out, &outlen), 1);
  CHECK_EQ (outlen, SHA256_DIGEST_LEN);
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
  static_assert (SHA256_DIGEST_LEN == uint256::NUM_BYTES,
                 "uint256 is not a valid output for SHA-256");

  CHECK (state != nullptr);

  unsigned char data[SHA256_DIGEST_LEN];
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
