// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_SIGNATURES_HPP
#define GAMECHANNEL_SIGNATURES_HPP

#include "proto/metadata.pb.h"
#include "proto/signatures.pb.h"

#include <xayagame/rpc-stubs/xayarpcclient.h>
#include <xayagame/rpc-stubs/xayawalletrpcclient.h>
#include <xayautil/uint256.hpp>

#include <set>
#include <string>

namespace xaya
{

/* ************************************************************************** */

/**
 * General interface for a signature scheme, implementing verification
 * of signatures (with address recovery).  This can be implemented using
 * Xaya Core's verifymessage RPC method, via Ethereum message signing, or
 * in principle by any other custom scheme as well.
 */
class SignatureVerifier
{

public:

  SignatureVerifier () = default;
  virtual ~SignatureVerifier () = default;

  /**
   * Returns the address which signed a given message as per the
   * signature.  In case the signature is entirely invalid (e.g. malformed),
   * this should return some invalid address for the signing scheme (e.g.
   * just the empty string or "invalid").
   */
  virtual std::string RecoverSigner (const std::string& msg,
                                     const std::string& sgn) const = 0;

};

/**
 * General interface for a signature scheme that supports signing
 * of messages with a particular address (holding the corresponding key).
 */
class SignatureSigner
{

public:

  SignatureSigner () = default;
  virtual ~SignatureSigner () = default;

  /**
   * Returns the address for which this instance can sign.
   */
  virtual std::string GetAddress () const = 0;

  /**
   * Signs a message with the underlying address.
   */
  virtual std::string SignMessage (const std::string& msg) = 0;

};

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

  /** The underlying RPC client for verification.  */
  XayaRpcClient& rpc;

public:

  explicit RpcSignatureVerifier (XayaRpcClient& r)
    : rpc(r)
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

  /** The underlying RPC wallet for signing.  */
  XayaWalletRpcClient& wallet;

  /** The address used for signing (must be in the wallet).  */
  const std::string address;

public:

  explicit RpcSignatureSigner (XayaWalletRpcClient& w, const std::string& addr);

  std::string GetAddress () const override;
  std::string SignMessage (const std::string& msg) override;

};

/* ************************************************************************** */

/**
 * Constructs the message (as string) that will be passed to "signmessage"
 * for the given channel, topic and raw data to sign.
 *
 * The topic string describes what the data is, so that e.g. a signed state
 * cannot be mistaken as a signed message stating the winner.  This string
 * must not contain any nul bytes.  "state" and "move" are reserved for use
 * with a game-specific BoardState and BoardMove value, respectively.  Other
 * values can be used for game-specific needs.
 */
std::string GetChannelSignatureMessage (const uint256& channelId,
                                        const proto::ChannelMetadata& meta,
                                        const std::string& topic,
                                        const std::string& data);

/**
 * Verifies the signatures on a SignedData instance in relation to the
 * participants and their signing keys of the given channel metadata.
 * This function returns a set of the participant indices for which a valid
 * signature was found on the data.
 *
 * The topic string describes what the data is, so that e.g. a signed state
 * cannot be mistaken as a signed message stating the winner.  This string
 * must not contain any nul bytes.  "state" and "move" are reserved for use
 * with a game-specific BoardState and BoardMove value, respectively.  Other
 * values can be used for game-specific needs.
 */
std::set<int> VerifyParticipantSignatures (const SignatureVerifier& verifier,
                                           const uint256& channelId,
                                           const proto::ChannelMetadata& meta,
                                           const std::string& topic,
                                           const proto::SignedData& data);

/**
 * Tries to sign the given data for the given participant index, using
 * the provided signer.  Returns true if a signature could be made.
 */
bool SignDataForParticipant (SignatureSigner& signer,
                             const uint256& channelId,
                             const proto::ChannelMetadata& meta,
                             const std::string& topic,
                             int index,
                             proto::SignedData& data);

/* ************************************************************************** */

} // namespace xaya

#endif // GAMECHANNEL_SIGNATURES_HPP
