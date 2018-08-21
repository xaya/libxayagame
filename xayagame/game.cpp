// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include <glog/logging.h>

namespace xaya
{

Game::Game (const std::string& id)
  : gameId(id)
{
  zmq.AddListener (gameId, this);
}

jsonrpc::clientVersion_t Game::rpcClientVersion = jsonrpc::JSONRPC_CLIENT_V1;

void
Game::BlockAttach (const std::string& id, const Json::Value& data,
                   const bool seqMismatch)
{
  CHECK_EQ (id, gameId);
  LOG (INFO) << "Attached:\n" << data;
}

void
Game::BlockDetach (const std::string& id, const Json::Value& data,
                   const bool seqMismatch)
{
  CHECK_EQ (id, gameId);
  LOG (INFO) << "Detached:\n" << data;
}

void
Game::ConnectRpcClient (jsonrpc::IClientConnector& conn)
{
  auto newClient = std::make_unique<XayaRpcClient> (conn, rpcClientVersion);

  std::lock_guard<std::mutex> lock(mutRpcClient);
  rpcClient = std::move (newClient);
}

bool
Game::DetectZmqEndpoint ()
{
  Json::Value notifications;

  {
    std::lock_guard<std::mutex> lock(mutRpcClient);
    CHECK (rpcClient != nullptr) << "RPC client is not yet set up";
    notifications = rpcClient->getzmqnotifications ();
  }
  VLOG (1) << "Configured ZMQ notifications:\n" << notifications;

  for (const auto& val : notifications)
    if (val.get ("type", "") == "pubgameblocks")
      {
        const std::string endpoint = val.get ("address", "").asString ();
        CHECK (!endpoint.empty ());
        LOG (INFO) << "Detected ZMQ endpoint: " << endpoint;
        SetZmqEndpoint (endpoint);
        return true;
      }
  return false;
}

void
Game::Run ()
{
  const bool zmqStarted = zmq.IsEndpointSet ();
  internal::MainLoop::Functor startAction = [this, zmqStarted] ()
    {
      if (zmqStarted)
        StartZmq ();
      else
        LOG (INFO)
            << "No ZMQ endpoint is set, not starting ZMQ from Game::Run()";
    };
  internal::MainLoop::Functor stopAction = [this, zmqStarted] ()
    {
      if (zmqStarted)
        StopZmq ();
    };
  mainLoop.Run (startAction, stopAction);
}

} // namespace xaya
