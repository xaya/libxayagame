// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include <xayagame/signatures.hpp>
#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <jsonrpccpp/common/exception.h>

#include <glog/logging.h>

#include <string>

namespace xaya
{

/* ************************************************************************** */

std::string
RpcSignatureVerifier::RecoverSigner (const std::string& msg,
                                     const std::string& sgn) const
{
  return VerifyMessage (rpc, msg, EncodeBase64 (sgn));
}

RpcSignatureSigner::RpcSignatureSigner (XayaWalletRpcClient& w,
                                        const std::string& addr)
  : wallet(w), address(addr)
{
  const auto info = wallet.getaddressinfo (address);
  CHECK (info.isObject ());
  const auto& mineVal = info["ismine"];
  CHECK (mineVal.isBool ());
  CHECK (mineVal.asBool ())
      << "Address " << address
      << " for signing is not owned by wallet RPC client";
}

std::string
RpcSignatureSigner::GetAddress () const
{
  return address;
}

std::string
RpcSignatureSigner::SignMessage (const std::string& msg)
{
  const std::string sgn = wallet.signmessage (address, msg);
  std::string decoded;
  CHECK (DecodeBase64 (sgn, decoded));
  return decoded;
}

/* ************************************************************************** */

std::string
GetChannelSignatureMessage (const uint256& channelId,
                            const proto::ChannelMetadata& meta,
                            const std::string& topic,
                            const std::string& data)
{
  CHECK_EQ (topic.find ('\0'), std::string::npos)
      << "Topic string contains nul character";
  const std::string nulByte("\0", 1);

  SHA256 hasher;
  hasher << channelId;
  hasher << EncodeBase64 (meta.reinit ()) << nulByte;
  hasher << topic << nulByte;
  hasher << data;

  return hasher.Finalise ().ToHex ();
}

std::set<int>
VerifyParticipantSignatures (const SignatureVerifier& verifier,
                             const uint256& channelId,
                             const proto::ChannelMetadata& meta,
                             const std::string& topic,
                             const proto::SignedData& data)
{
  const std::string msg = GetChannelSignatureMessage (channelId, meta, topic,
                                                      data.data ());

  std::set<std::string> addresses;
  for (const auto& sgn : data.signatures ())
    addresses.emplace (verifier.RecoverSigner (msg, sgn));

  std::set<int> res;
  for (int i = 0; i < meta.participants_size (); ++i)
    if (addresses.count (meta.participants (i).address ()) > 0)
      res.emplace (i);

  return res;
}

bool
SignDataForParticipant (SignatureSigner& signer,
                        const uint256& channelId,
                        const proto::ChannelMetadata& meta,
                        const std::string& topic,
                        const int index,
                        proto::SignedData& data)
{
  CHECK_GE (index, 0);
  CHECK_LT (index, meta.participants_size ());
  const std::string& addr = meta.participants (index).address ();
  LOG (INFO) << "Trying to sign data with address " << addr << "...";

  if (addr != signer.GetAddress ())
    {
      LOG (ERROR) << "The provided signer is for a different address";
      return false;
    }

  const auto msg
      = GetChannelSignatureMessage (channelId, meta, topic, data.data ());
  data.add_signatures (signer.SignMessage (msg));
  return true;
}

} // namespace xaya
