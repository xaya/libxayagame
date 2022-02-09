// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_ETHSIGNATURES_HPP
#define GAMECHANNEL_ETHSIGNATURES_HPP

#include "signatures.hpp"

#include <eth-utils/ecdsa.hpp>

#include <string>

namespace xaya
{

/**
 * An implementation of the verifier based on Ethereum signatures.  Note that
 * the signatures are raw strings of 65 bytes, not hex-strings as used
 * typically.
 *
 * This signing scheme is self-contained and does not rely on an RPC connection
 * or any other external resources (unlike the Xaya Core based message
 * signing from rpcwallet).  It happens to be based on Ethereum addresses and
 * the Ethereum message signing scheme, but can be applied to any channel
 * applications (not just ones built on top of an Ethereum-like blockchain).
 */
class EthSignatureVerifier : public SignatureVerifier
{

private:

  /** The underlying eth-utils ECDSA context.  */
  const ethutils::ECDSA& ctx;

public:

  explicit EthSignatureVerifier (const ethutils::ECDSA& c)
    : ctx(c)
  {}

  std::string RecoverSigner (const std::string& msg,
                             const std::string& sgn) const override;

};

/**
 * An implementation of the signer based on an Ethereum private key
 * and Ethereum signatures.  The private key is held in memory through the
 * eth-utils ECDSA::Key.
 */
class EthSignatureSigner : public SignatureSigner
{

private:

  /** The underlying eth-utils ECDSA context.  */
  const ethutils::ECDSA& ctx;

  /** The private key used for signing.  */
  ethutils::ECDSA::Key key;

public:

  /**
   * Constructs the signer based on a given private key (either as raw
   * string of 32 bytes, or hex-string with 0x prefix).
   */
  explicit EthSignatureSigner (const ethutils::ECDSA& c, const std::string& k);

  std::string GetAddress () const override;
  std::string SignMessage (const std::string& msg) override;

};

} // namespace xaya

#endif // GAMECHANNEL_ETHSIGNATURES_HPP
