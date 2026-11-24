// Copyright (C) 2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_RPCPROVIDER_HPP
#define XAYAGAME_RPCPROVIDER_HPP

#include "rpc-stubs/xayarpcclient.h"
#include "rpc-stubs/xayawalletrpcclient.h"

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace xaya
{

/**
 * Settings for a JSON-RPC connection and a utility class to provide a client
 * based on them.  A separate client is kept for each thread, so that clients
 * can be used in parallel.
 */
template <typename Client>
  class GenericRpcProvider
{

private:

  /** The connection URL (we always use HTTP).  */
  std::string url;

  /** The protocol version to use.  */
  jsonrpc::clientVersion_t version;

  /**
   * The data about the RPC client used for one particular thread.
   */
  struct PerThreadData
  {

    /** The underlying HTTP client.  */
    jsonrpc::HttpClient httpClient;

    /** The actual RPC client.  */
    Client rpcClient;

    PerThreadData (const std::string& u, const jsonrpc::clientVersion_t v)
      : httpClient(u), rpcClient(httpClient, v)
    {}

  };

  /** Mutex guarding threadData.  */
  mutable std::mutex dataMut;

  /** Per-thread clients, keyed by the ID of the thread using them.  */
  mutable std::map<std::thread::id, PerThreadData> threadData;

  /**
   * Helper function that returns the current thread's data, constructing it
   * if it does not exist yet.
   */
  PerThreadData& GetThreadData () const;

public:

  GenericRpcProvider () = default;

  /**
   * Sets the connection settings.  Can only be done once and the settings
   * cannot be changed afterwards (so that existing per-thread clients
   * remain valid).
   */
  void Set (const std::string& u, jsonrpc::clientVersion_t v);

  /**
   * Checks if the settings are provided.
   */
  operator bool () const;

  /**
   * Returns the RPC client.  Must only be called if the settings are provided.
   * This returns a per-thread instance owned by the provider, so that it is
   * safe to use this from multiple threads and use the returned RPC clients
   * in parallel.  The provider must outlive all threads that use it; all
   * clients are destroyed with the provider.
   */
  Client& operator* () const;

};

using XayaRpcProvider = GenericRpcProvider<XayaRpcClient>;
using XayaWalletRpcProvider = GenericRpcProvider<XayaWalletRpcClient>;

} // namespace xaya

#include "rpcprovider.tpp"

#endif // XAYAGAME_RPCPROVIDER_HPP
