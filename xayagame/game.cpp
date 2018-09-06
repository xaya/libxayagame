// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include <glog/logging.h>

#include <sstream>

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

Json::Value
GameLogic::GameStateToJson (const GameStateData& state)
{
  return state;
}

Game::Game (const std::string& id)
  : gameId(id)
{
  zmq.AddListener (gameId, this);
}

jsonrpc::clientVersion_t Game::rpcClientVersion = jsonrpc::JSONRPC_CLIENT_V1;

std::string
Game::StateToString (const State s)
{
  switch (s)
    {
    case State::UNKNOWN:
      return "unknown";
    case State::PREGENESIS:
      return "pregenesis";
    case State::OUT_OF_SYNC:
      return "out-of-sync";
    case State::CATCHING_UP:
      return "catching-up";
    case State::UP_TO_DATE:
      return "up-to-date";
    }

  std::ostringstream out;
  out << "invalid-" << static_cast<int> (s);
  return out.str ();
}

bool
Game::UpdateStateForAttach (const uint256& parent, const uint256& hash,
                            const Json::Value& blockData)
{
  uint256 currentHash;
  CHECK (storage->GetCurrentBlockHash (currentHash));
  if (currentHash != parent)
    {
      LOG (WARNING)
          << "Game state hash " << currentHash.ToHex ()
          << " does not match attached block's parent " << parent.ToHex ();
      return false;
    }

  const GameStateData oldState = storage->GetCurrentGameState ();
  UndoData undo;
  const GameStateData newState
      = rules->ProcessForward (oldState, blockData, undo);

  const unsigned height = blockData["block"]["height"].asUInt ();

  storage->SetCurrentGameState (hash, newState);
  storage->AddUndoData (hash, height, undo);
  LOG (INFO)
      << "Current game state is at height " << height
      << " (block " << hash.ToHex () << ")";

  return true;
}

bool
Game::UpdateStateForDetach (const uint256& parent, const uint256& hash,
                            const Json::Value& blockData)
{
  uint256 currentHash;
  CHECK (storage->GetCurrentBlockHash (currentHash));
  if (currentHash != hash)
    {
      LOG (WARNING)
          << "Game state hash " << currentHash.ToHex ()
          << " does not match detached block's hash " << hash.ToHex ();
      return false;
    }

  UndoData undo;
  if (!storage->GetUndoData (hash, undo))
    {
      LOG (ERROR)
          << "Failed to retrieve undo data for block " << hash.ToHex ()
          << ".  Need to resync from scratch.";
      storage->Clear ();
      return false;
    }

  const GameStateData newState = storage->GetCurrentGameState ();
  const GameStateData oldState
      = rules->ProcessBackwards (newState, blockData, undo);

  storage->SetCurrentGameState (parent, oldState);
  storage->ReleaseUndoData (hash);
  LOG (INFO)
      << "Detached " << hash.ToHex () << ", restored state for block "
      << parent.ToHex ();

  return true;
}

bool
Game::IsReqtokenRelevant (const Json::Value& data) const
{
  std::string msgReqToken;
  if (data.isMember ("reqtoken"))
    msgReqToken = data["reqtoken"].asString ();

  if (state == State::CATCHING_UP)
    return msgReqToken == reqToken;

  return msgReqToken.empty ();
}

void
Game::BlockAttach (const std::string& id, const Json::Value& data,
                   const bool seqMismatch)
{
  CHECK_EQ (id, gameId);
  VLOG (2) << "Attached:\n" << data;

  uint256 parent;
  CHECK (parent.FromHex (data["block"]["parent"].asString ()));
  uint256 hash;
  CHECK (hash.FromHex (data["block"]["hash"].asString ()));
  VLOG (1) << "Attaching block " << hash.ToHex ();

  std::lock_guard<std::mutex> lock(mut);

  /* If we missed notifications, always reinitialise the state to make sure
     that all is again consistent.  */
  if (seqMismatch)
    {
      LOG (WARNING) << "Missed ZMQ notifications, reinitialising state";
      ReinitialiseState ();
      return;
    }

  /* Ignore notifications that are not relevant at the moment.  */
  if (!IsReqtokenRelevant (data))
    {
      VLOG (1) << "Ignoring irrelevant attach notification";
      return;
    }

  /* Handle the notification depending on the current state.  */
  bool needReinit = false;
  switch (state)
    {
    case State::PREGENESIS:
      /* Check if we have reached the game's genesis height.  If we have,
         reinitialise which will store the initial game state.  */
      if (hash == targetBlockHash)
        needReinit = true;
      break;

    case State::CATCHING_UP:
      if (!UpdateStateForAttach (parent, hash, data))
        needReinit = true;

      /* If we are now at the last catching-up's target block hash,
         reinitialise the state as well.  This will check the current best
         tip and set the state to UP_TO_DATE or request more updates.  */
      if (hash == targetBlockHash)
        needReinit = true;

      break;

    case State::UP_TO_DATE:
      if (!UpdateStateForAttach (parent, hash, data))
        needReinit = true;
      break;

    case State::UNKNOWN:
    case State::OUT_OF_SYNC:
    default:
      LOG (FATAL) << "Unexpected state: " << StateToString (state);
      break;
    }

  if (needReinit)
    ReinitialiseState ();
}

void
Game::BlockDetach (const std::string& id, const Json::Value& data,
                   const bool seqMismatch)
{
  CHECK_EQ (id, gameId);
  VLOG (2) << "Detached:\n" << data;

  uint256 parent;
  CHECK (parent.FromHex (data["block"]["parent"].asString ()));
  uint256 hash;
  CHECK (hash.FromHex (data["block"]["hash"].asString ()));
  VLOG (1) << "Detaching block " << hash.ToHex ();

  std::lock_guard<std::mutex> lock(mut);

  /* If we missed notifications, always reinitialise the state to make sure
     that all is again consistent.  */
  if (seqMismatch)
    {
      LOG (WARNING) << "Missed ZMQ notifications, reinitialising state";
      ReinitialiseState ();
      return;
    }

  /* Ignore notifications that are not relevant at the moment.  */
  if (!IsReqtokenRelevant (data))
    {
      VLOG (1) << "Ignoring irrelevant detach notification";
      return;
    }

  /* Handle the notification depending on the current state.  */
  bool needReinit = false;
  switch (state)
    {
    case State::PREGENESIS:
      /* Detaches are irrelevant (and unlikely).  */
      break;

    case State::CATCHING_UP:
      if (!UpdateStateForDetach (parent, hash, data))
        needReinit = true;

      /* We may reach a catching-up target also when detaching blocks.  This
         happens, for instance, when a block was declared invalid and a couple
         of blocks was just detached.  If a ZMQ message is missed at the
         same time (*or if this was the very first detach notification*!),
         then the client is catching-up while only detaching.  */
      if (parent == targetBlockHash)
        needReinit = true;

      break;

    case State::UP_TO_DATE:
      if (!UpdateStateForDetach (parent, hash, data))
        needReinit = true;
      break;

    case State::UNKNOWN:
    case State::OUT_OF_SYNC:
    default:
      LOG (FATAL) << "Unexpected state: " << StateToString (state);
      break;
    }

  if (needReinit)
    ReinitialiseState ();
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

Json::Value
Game::GetCurrentJsonState () const
{
  std::unique_lock<std::mutex> lock(mut);

  Json::Value res(Json::objectValue);
  res["gameid"] = gameId;
  res["chain"] = chain;
  res["state"] = StateToString (state);

  uint256 hash;
  if (storage->GetCurrentBlockHash (hash))
    {
      res["blockhash"] = hash.ToHex ();

      const GameStateData gameState = storage->GetCurrentGameState ();
      res["gamestate"] = rules->GameStateToJson (gameState);
    }

  return res;
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
Game::SyncFromCurrentState (const Json::Value& blockchainInfo,
                            const uint256& currentHash)
{
  CHECK (state == State::OUT_OF_SYNC);

  uint256 daemonBestHash;
  CHECK (daemonBestHash.FromHex (blockchainInfo["bestblockhash"].asString ()));

  if (daemonBestHash == currentHash)
    {
      LOG (INFO) << "Game state matches current tip, we are up-to-date";
      state = State::UP_TO_DATE;
      return;
    }

  LOG (INFO)
      << "Game state does not match current tip, requesting updates from "
      << currentHash.ToHex ();
  const Json::Value upd
      = rpcClient->game_sendupdates (currentHash.ToHex (), gameId);

  LOG (INFO)
      << "Retrieving " << upd["steps"]["detach"].asInt () << " detach and "
      << upd["steps"]["attach"].asInt () << " attach steps with reqtoken = "
      << upd["reqtoken"].asString ()
      << ", leading to block " << upd["toblock"].asString ();

  state = State::CATCHING_UP;
  CHECK (targetBlockHash.FromHex (upd["toblock"].asString ()));
  reqToken = upd["reqtoken"].asString ();
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
      LOG (INFO) << "We have a current game state, syncing from there";
      state = State::OUT_OF_SYNC;
      SyncFromCurrentState (data, currentHash);
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
      targetBlockHash = genesisHash;
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
  LOG (INFO)
      << "We are at the genesis height, storing initial game state for block "
      << genesisHash.ToHex ();
  state = State::OUT_OF_SYNC;
  SyncFromCurrentState (data, genesisHash);
}

} // namespace xaya
