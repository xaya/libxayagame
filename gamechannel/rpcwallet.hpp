// Copyright (C) 2019-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_RPCWALLET_HPP
#define GAMECHANNEL_RPCWALLET_HPP

#include "movesender.hpp"
#include "signatures.hpp"

#include <xayagame/rpcprovider.hpp>

#include <string>

namespace xaya
{

/**
 * An implementation of the verifier based on a Xaya RPC connection.
 *
 * This uses Xaya Core's signmessage/verifymessage scheme, but signatures
 * returned and passed in for verification are assumed to be already base64
 * decoded to raw bytes.
 */
class RpcSignatureVerifier : public SignatureVerifier
{

private:

  /** The RPC provider used for verification.  */
  const XayaRpcProvider& provider;

public:

  explicit RpcSignatureVerifier (const XayaRpcProvider& p)
    : provider(p)
  {}

  std::string RecoverSigner (const std::string& msg,
                             const std::string& sgn) const override;

};

/**
 * An implementation of the signer based on a Xaya RPC connection.
 */
class RpcSignatureSigner : public SignatureSigner
{

private:

  /** The RPC wallet provider used for signing.  */
  const XayaWalletRpcProvider& wallet;

  /** The address used for signing (must be in the wallet).  */
  const std::string address;

public:

  explicit RpcSignatureSigner (const XayaWalletRpcProvider& w,
                               const std::string& addr);

  std::string GetAddress () const override;
  std::string SignMessage (const std::string& msg) override;

};

/**
 * A concrete implementation of TransactionSender that uses a Xaya Core RPC
 * connection with name_update.
 */
class RpcTransactionSender : public TransactionSender
{

private:

  /** Xaya RPC provider to use.  */
  const XayaRpcProvider& rpc;

  /** Xaya wallet RPC provider to use.  */
  const XayaWalletRpcProvider& wallet;

public:

  explicit RpcTransactionSender (const XayaRpcProvider& r,
                                 const XayaWalletRpcProvider& w)
    : rpc(r), wallet(w)
  {}

  uint256 SendRawMove (const std::string& name,
                       const std::string& value) override;
  bool IsPending (const uint256& txid) const override;

};

} // namespace xaya

#endif // GAMECHANNEL_RPCWALLET_HPP
