// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcwallet.hpp"

#include <xayagame/signatures.hpp>
#include <xayautil/base64.hpp>

#include <glog/logging.h>

namespace xaya
{

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

uint256
RpcTransactionSender::SendRawMove (const std::string& name,
                                   const std::string& value)
{
  const std::string fullName = "p/" + name;
  const std::string txidHex = wallet.name_update (fullName, value);

  uint256 txid;
  CHECK (txid.FromHex (txidHex));

  return txid;
}

bool
RpcTransactionSender::IsPending (const uint256& txid) const
{
  const std::string txidHex = txid.ToHex ();

  const auto mempool = rpc.getrawmempool ();
  for (const auto& tx : mempool)
    if (tx.asString () == txidHex)
      return true;

  return false;
}

} // namespace xaya
