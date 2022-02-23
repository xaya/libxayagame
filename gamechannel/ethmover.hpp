// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_ETHMOVER_HPP
#define GAMECHANNEL_ETHMOVER_HPP

#include "movesender.hpp"

#include "rpc-stubs/ethrpcclient.h"

#include <string>

namespace xaya
{

/**
 * An implementation of TransactionSender, which sends moves through
 * the XayaAccounts smart-contract deployed on an EVM chain.
 *
 * Note that this only works if the associated "from" address owns the names
 * for moves or has operator rights for them, and the private key for this
 * address is known to the RPC endpoint's local wallet.  As such it is
 * mostly useful for testing with Ganache, rather than a real production
 * setting.
 */
class EthTransactionSender : public TransactionSender
{

private:

  /** Ethereum RPC connection.  */
  EthRpcClient& rpc;

  /** The address to send transactions from.  */
  const std::string from;

  /** The XayaAccounts contract address.  */
  const std::string contract;

  /** The ABI function selector for XayaAccounts::move.  */
  const std::string moveFcn;

public:

  explicit EthTransactionSender (EthRpcClient& r, const std::string& f,
                                 const std::string& c);

  uint256 SendRawMove (const std::string& name,
                       const std::string& value) override;
  bool IsPending (const uint256& txid) const override;

};

} // namespace xaya

#endif // GAMECHANNEL_ETHMOVER_HPP
