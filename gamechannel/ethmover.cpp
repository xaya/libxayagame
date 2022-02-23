// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ethmover.hpp"

#include <eth-utils/abi.hpp>
#include <eth-utils/hexutils.hpp>
#include <eth-utils/keccak.hpp>

#include <glog/logging.h>

namespace xaya
{

namespace
{

/**
 * Computes and returns the ABI function selector for the move function
 * on the XayaAccounts contract.
 */
std::string
MoveFunctionSelector ()
{
  static const char* FCN = "move(string,string,string,uint256,uint256,address)";
  const std::string hash = ethutils::Keccak256 (FCN);
  return "0x" + ethutils::Hexlify (hash).substr (0, 8);
}

} // anonymous namespace

EthTransactionSender::EthTransactionSender (
    EthRpcClient& r, const std::string& f, const std::string& c)
  : rpc(r), from(f), contract(c), moveFcn(MoveFunctionSelector ())
{}

uint256
EthTransactionSender::SendRawMove (const std::string& name,
                                   const std::string& value)
{
  ethutils::AbiEncoder enc(6);
  enc.WriteBytes ("0x" + ethutils::Hexlify ("p"));
  enc.WriteBytes ("0x" + ethutils::Hexlify (name));
  enc.WriteBytes ("0x" + ethutils::Hexlify (value));
  enc.WriteWord ("0x" + std::string (64, 'f'));
  enc.WriteWord ("0x" + std::string (64, '0'));
  enc.WriteWord ("0x" + std::string (64, '0'));

  Json::Value tx(Json::objectValue);
  tx["from"] = from;
  tx["to"] = contract;
  tx["data"] = ethutils::AbiEncoder::ConcatHex (moveFcn, enc.Finalise ());
  tx["gas"] = rpc.eth_estimateGas (tx);

  const std::string txid = rpc.eth_sendTransaction (tx);
  CHECK_EQ (txid.substr (0, 2), "0x")
      << "Invalid hex as txid returned: " << txid;
  uint256 res;
  CHECK (res.FromHex (txid.substr (2)))
      << "Invalid hex as txid returned: " << txid;

  return res;
}

bool
EthTransactionSender::IsPending (const uint256& txid) const
{
  /* We can't call eth_getTransactionByHash through the generated stub,
     because the method may return either an object or null (if the tx is not
     found).  The stub expects all calls to return the same underlying "type".
     But we can just fake what the stub does manually very easily.  */

  Json::Value p(Json::arrayValue);
  p.append ("0x" + txid.ToHex ());

  const Json::Value result = rpc.CallMethod ("eth_getTransactionByHash", p);

  if (!result.isNull ())
    {
      /* The transaction was not found, which means it is certainly
         not pending at the moment.  */
      return false;
    }

  if (!result.isObject ())
    {
      LOG (WARNING)
          << "Unexpected result from eth_getTransactionByHash: " << result;
      return false;
    }

  const auto& blkVal = result["blockHash"];
  if (blkVal.isNull ())
    return true;

  LOG_IF (WARNING, !blkVal.isString ())
      << "Unexpected 'blockHash' from eth_getTransactionByHash: " << result;
  return false;
}

} // namespace xaya
