// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include <glog/logging.h>

namespace xaya
{

const std::string&
GameLogic::GetChain () const
{
  CHECK (!chain.empty ());
  return chain;
}

void
GameLogic::SetChain (const std::string& c)
{
  CHECK (chain.empty () || chain == c);
  chain = c;
}

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
  VLOG (2) << "Attached:\n" << data;

  uint256 parent;
  CHECK (parent.FromHex (data["parent"].asString ()));
  uint256 child;
  CHECK (child.FromHex (data["child"].asString ()));
  VLOG (1) << "Attaching block " << child.ToHex ();

  std::lock_guard<std::mutex> lock(mut);
  switch (state)
    {
    case State::UNKNOWN:
      LOG (FATAL) << "UNKNOWN state in ZMQ message handler";
      return;

    case State::PREGENESIS:
      /* If we reached the hash we have been waiting for, reinitialise the
         state; this will store the game's genesis state and continue syncing
         from there.  If we missed ZMQ notifications, do the same -- we might
         have missed the genesis hash.  */
      if (child == gameGenesisHash || seqMismatch)
        ReinitialiseState ();
      return;

    case State::OUT_OF_SYNC:
      /* TODO: Handle this case in the future.  */
      return;
    }

  LOG (DFATAL)
      << "State " << static_cast<int> (state) << " not handled correctly";
}

void
Game::BlockDetach (const std::string& id, const Json::Value& data,
                   const bool seqMismatch)
{
  CHECK_EQ (id, gameId);
  VLOG (2) << "Detached:\n" << data;

  uint256 parent;
  CHECK (parent.FromHex (data["parent"].asString ()));
  uint256 child;
  CHECK (child.FromHex (data["child"].asString ()));
  VLOG (1) << "Detaching block " << child.ToHex ();
}

void
Game::ConnectRpcClient (jsonrpc::IClientConnector& conn)
{
  auto newClient = std::make_unique<XayaRpcClient> (conn, rpcClientVersion);

  std::lock_guard<std::mutex> lock(mut);
  rpcClient = std::move (newClient);

  const Json::Value info = rpcClient->getblockchaininfo ();
  const std::string newChain = info["chain"].asString ();
  CHECK (chain.empty () || chain == newChain)
      << "Previous RPC connection had chain " << chain << ", now we have "
      << newChain;
  chain = newChain;
  LOG (INFO) << "Connected to RPC daemon with chain " << chain;

  if (rules != nullptr)
    rules->SetChain (chain);
}

const std::string&
Game::GetChain () const
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (!chain.empty ());
  return chain;
}

void
Game::SetStorage (StorageInterface* s)
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (!mainLoop.IsRunning ());
  storage = s;
}

void
Game::SetGameLogic (GameLogic* gl)
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (!mainLoop.IsRunning ());
  rules = gl;
  if (!chain.empty ())
    rules->SetChain (chain);
}

bool
Game::DetectZmqEndpoint ()
{
  Json::Value notifications;

  {
    std::lock_guard<std::mutex> lock(mut);
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
Game::TrackGame ()
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (rpcClient != nullptr) << "RPC client is not yet set up";
  rpcClient->trackedgames ("add", gameId);
  LOG (INFO) << "Added " << gameId << " to tracked games";
}

void
Game::UntrackGame ()
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (rpcClient != nullptr) << "RPC client is not yet set up";
  rpcClient->trackedgames ("remove", gameId);
  LOG (INFO) << "Removed " << gameId << " from tracked games";
}

void
Game::Start ()
{
  TrackGame ();
  zmq.Start ();

  std::lock_guard<std::mutex> lock(mut);
  ReinitialiseState ();
}

void
Game::Stop ()
{
  zmq.Stop ();
  UntrackGame ();
}

void
Game::Run ()
{
  CHECK (storage != nullptr && rules != nullptr)
      << "Storage and GameLogic must be set before starting the main loop";

  internal::MainLoop::Functor startAction = [this] () { Start (); };
  internal::MainLoop::Functor stopAction = [this] () { Stop (); };

  mainLoop.Run (startAction, stopAction);
}

void
Game::ReinitialiseState ()
{
  state = State::UNKNOWN;
  LOG (INFO) << "Reinitialising game state";

  const Json::Value data = rpcClient->getblockchaininfo ();

  uint256 currentHash;
  if (storage->GetCurrentBlockHash (currentHash))
    {
      LOG (INFO) << "We have a current game state";
      state = State::OUT_OF_SYNC;
      /* TODO: Support catching up (and not just getting to an initial state)
         in the future.  */
      return;
    }

  /* We do not have a current state in the storage.  This means that we have
     to reset to the initial state.  */

  unsigned genesisHeight;
  std::string genesisHashHex;
  const GameStateData genesisData
      = rules->GetInitialState (genesisHeight, genesisHashHex);
  uint256 genesisHash;
  CHECK (genesisHash.FromHex (genesisHashHex));

  /* If the current block height in the daemon is not yet the game's genesis
     height, simply wait for the genesis hash to be attached.  */
  if (data["blocks"].asUInt () < genesisHeight)
    {
      LOG (INFO)
          << "Block height " << data["blocks"].asInt ()
          << " is before the genesis height " << genesisHeight;
      state = State::PREGENESIS;
      gameGenesisHash = genesisHash;
      return;
    }

  /* Otherwise, we can store the initial state and start to sync from there.  */
  const std::string blockHashHex = rpcClient->getblockhash (genesisHeight);
  uint256 blockHash;
  CHECK (blockHash.FromHex (blockHashHex));
  CHECK (blockHash == genesisHash)
    << "The game's genesis block hash and height do not match";
  storage->Clear ();
  storage->SetCurrentGameState (genesisHash, genesisData);
  LOG (INFO) << "We are at the genesis height, storing initial game state";
  state = State::OUT_OF_SYNC;
  /* TODO: Start catching up here.  */
}

} // namespace xaya
