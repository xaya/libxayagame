// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ethsignatures.hpp"

#include <eth-utils/hexutils.hpp>

#include <glog/logging.h>

namespace xaya
{

std::string
EthSignatureVerifier::RecoverSigner (const std::string& msg,
                                     const std::string& sgn) const
{
  const std::string sgnHex = "0x" + ethutils::Hexlify (sgn);
  const ethutils::Address addr = ctx.VerifyMessage (msg, sgnHex);
  return addr ? addr.GetChecksummed () : "invalid";
}

EthSignatureSigner::EthSignatureSigner (const ethutils::ECDSA& c,
                                        const std::string& k)
  : ctx(c), key(ctx.SecretKey (k))
{
  CHECK (key) << "Invalid private key passed to signer";
  LOG (INFO) << "Private key passed to signer is for address " << GetAddress ();
}

std::string
EthSignatureSigner::GetAddress () const
{
  return key.GetAddress ().GetChecksummed ();
}

std::string
EthSignatureSigner::SignMessage (const std::string& msg)
{
  const std::string sgnHex = ctx.SignMessage (msg, key);
  CHECK_EQ (sgnHex.substr (0, 2), "0x")
      << "Signature from eth-utils is not hex: " << sgnHex;

  std::string sgn;
  CHECK (ethutils::Unhexlify (sgnHex.substr (2), sgn))
      << "Signature from eth-utils is not hex: " << sgnHex;
  CHECK_EQ (sgn.size (), 65)
      << "Signature from eth-utils has unexpected size: " << sgnHex;

  return sgn;
}

} // namespace xaya
