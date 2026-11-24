// Copyright (C) 2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Template implementation code for rpcprovider.hpp.  */

#include <glog/logging.h>

namespace xaya
{

template <typename Client>
  typename GenericRpcProvider<Client>::PerThreadData&
  GenericRpcProvider<Client>::GetThreadData () const
{
  std::lock_guard<std::mutex> lock (dataMut);

  const auto res = threadData.try_emplace (std::this_thread::get_id (),
                                           url, version);
  return res.first->second;
}

template <typename Client>
  void
  GenericRpcProvider<Client>::Set (const std::string& u,
                                   const jsonrpc::clientVersion_t v)
{
  CHECK (url.empty ()) << "RPC provider is already configured";

  url = u;
  version = v;
}

template <typename Client>
  GenericRpcProvider<Client>::operator bool () const
{
  return !url.empty ();
}

template <typename Client>
  Client&
  GenericRpcProvider<Client>::operator* () const
{
  CHECK (!url.empty ()) << "RPC settings not configured";
  return GetThreadData ().rpcClient;
}

} // namespace xaya
