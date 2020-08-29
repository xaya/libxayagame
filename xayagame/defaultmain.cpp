// Copyright (C) 2018-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "defaultmain.hpp"

#include "gamerpcserver.hpp"
#include "lmdbstorage.hpp"
#include "sqlitestorage.hpp"

#include "rpc-stubs/xayarpcclient.h"

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/common/exception.h>

#include <glog/logging.h>

#include <experimental/filesystem>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <thread>

namespace xaya
{

std::unique_ptr<RpcServerInterface>
CustomisedInstanceFactory::BuildRpcServer (
    Game& game, jsonrpc::AbstractServerConnector& conn)
{
  std::unique_ptr<RpcServerInterface> res;
  res.reset (new WrappedRpcServer<GameRpcServer> (game, conn));
  return res;
}

std::vector<std::unique_ptr<GameComponent>>
CustomisedInstanceFactory::BuildGameComponents (Game& game)
{
  return {};
}

namespace
{

namespace fs = std::experimental::filesystem;

/**
 * Returns the directory in which data for this game should be stored.
 * The directory is created as needed.
 */
fs::path
GetGameDirectory (const GameDaemonConfiguration& config,
                  const std::string& gameId, const Chain chain)
{
  CHECK (!config.DataDirectory.empty ()) << "DataDirectory has not been set";
  const fs::path gameDir
      = fs::u8path (config.DataDirectory)
          / fs::u8path (gameId)
          / fs::u8path (ChainToString (chain));

  if (fs::is_directory (gameDir))
    LOG (INFO) << "Using existing data directory: " << gameDir;
  else
    {
      LOG (INFO) << "Creating data directory: " << gameDir;
      CHECK (fs::create_directories (gameDir));
    }

  return gameDir;
}

/**
 * Sets up a StorageInterface instance according to the configuration.
 */
std::unique_ptr<StorageInterface>
CreateStorage (const GameDaemonConfiguration& config,
               const std::string& gameId, const Chain chain)
{
  if (config.StorageType == "memory")
    return std::make_unique<MemoryStorage> ();

  const fs::path gameDir = GetGameDirectory (config, gameId, chain);

  if (config.StorageType == "lmdb")
    {
      const fs::path lmdbDir = gameDir / fs::path ("lmdb");
      if (!fs::is_directory (lmdbDir))
        {
          LOG (INFO) << "Creating directory for LMDB environment: " << lmdbDir;
          CHECK (fs::create_directories (lmdbDir));
        }
      return std::make_unique<LMDBStorage> (lmdbDir.string ());
    }

  if (config.StorageType == "sqlite")
    {
      const fs::path dbFile = gameDir / fs::path ("storage.sqlite");
      return std::make_unique<SQLiteStorage> (dbFile.u8string ());
    }

  LOG (FATAL) << "Invalid storage type selected: " << config.StorageType;
  return nullptr;
}

/**
 * Constructs the server connector for the JSON-RPC server (if any) based
 * on the configuration.
 */
std::unique_ptr<jsonrpc::AbstractServerConnector>
CreateRpcServerConnector (const GameDaemonConfiguration& config)
{
  switch (config.GameRpcServer)
    {
    case RpcServerType::NONE:
      return nullptr;

    case RpcServerType::HTTP:
      {
        CHECK (config.GameRpcPort != 0)
            << "GameRpcPort must be specified for HTTP server type";
        LOG (INFO)
            << "Starting JSON-RPC HTTP server at port " << config.GameRpcPort;
        auto srv = std::make_unique<jsonrpc::HttpServer> (config.GameRpcPort);
        if (config.GameRpcListenLocally)
          srv->BindLocalhost ();
        return srv;
      }
    }

  LOG (FATAL)
      << "Invalid GameRpcServer value: "
      << static_cast<int> (config.GameRpcServer);
}

/**
 * Checks the Xaya Core version against the minimum and maximum versions
 * defined in the configuration.
 */
void
VerifyXayaVersion (const GameDaemonConfiguration& config, const unsigned v)
{
  LOG (INFO) << "Connected to Xaya Core version " << v;
  CHECK_GE (v, config.MinXayaVersion) << "Xaya Core is too old";
  if (config.MaxXayaVersion > 0)
    CHECK_LE (v, config.MaxXayaVersion) << "Xaya Core is too new";
}

/**
 * Waits for the Xaya Core RPC interface to be available on the given
 * client connector.
 */
void
WaitForXaya (jsonrpc::IClientConnector& conn)
{
  LOG (INFO) << "Waiting for Xaya to be up...";

  XayaRpcClient client(conn, jsonrpc::JSONRPC_CLIENT_V1);
  while (true)
    try
      {
        client.getnetworkinfo ();
        LOG (INFO) << "Xaya Core is available now";
        break;
      }
    catch (const jsonrpc::JsonRpcException& exc)
      {
        VLOG (1) << exc.what ();
        LOG (INFO) << "Failed to connect to Xaya Core, waiting...";
        std::this_thread::sleep_for (std::chrono::seconds (1));
      }
}

} // anonymous namespace

int
DefaultMain (const GameDaemonConfiguration& config, const std::string& gameId,
             GameLogic& rules)
{
  try
    {
      CustomisedInstanceFactory* instanceFact = config.InstanceFactory;
      std::unique_ptr<CustomisedInstanceFactory> defaultInstanceFact;
      if (instanceFact == nullptr)
        {
          defaultInstanceFact = std::make_unique<CustomisedInstanceFactory> ();
          instanceFact = defaultInstanceFact.get ();
        }

      CHECK (!config.XayaRpcUrl.empty ()) << "XayaRpcUrl must be configured";
      const std::string jsonRpcUrl(config.XayaRpcUrl);
      jsonrpc::HttpClient httpConnector(jsonRpcUrl);

      if (config.XayaRpcWait)
        WaitForXaya (httpConnector);

      auto game = std::make_unique<Game> (gameId);
      game->ConnectRpcClient (httpConnector);
      VerifyXayaVersion (config, game->GetXayaVersion ());
      CHECK (game->DetectZmqEndpoint ());

      std::unique_ptr<StorageInterface> storage
          = CreateStorage (config, gameId, game->GetChain ());
      game->SetStorage (*storage);

      game->SetGameLogic (rules);

      if (config.PendingMoves != nullptr)
        game->SetPendingMoveProcessor (*config.PendingMoves);

      if (config.EnablePruning >= 0)
        game->EnablePruning (config.EnablePruning);

      auto components = instanceFact->BuildGameComponents (*game);

      auto serverConnector = CreateRpcServerConnector (config);
      if (serverConnector == nullptr)
          LOG (WARNING)
              << "No connector has been set up for the game RPC server,"
                 " no RPC interface will be available";
      else
          components.push_back (
              instanceFact->BuildRpcServer (*game, *serverConnector));

      for (auto& c : components)
        c->Start ();
      game->Run ();
      for (auto& c : components)
        c->Stop ();

      /* We need to make sure that the Game instance is destructed before the
         storage is.  That is necessary, since destructing the Game instance
         may still cause some batched transactions to be flushed, and this
         needs the storage intact.  */
      game.reset ();
    }
  catch (const std::exception& exc)
    {
      LOG (FATAL) << "Exception caught: " << exc.what ();
    }
  catch (...)
    {
      LOG (FATAL) << "Unknown exception caught";
    }

  return EXIT_SUCCESS;
}

int
SQLiteMain (const GameDaemonConfiguration& config, const std::string& gameId,
            SQLiteGame& rules)
{
  try
    {
      CustomisedInstanceFactory* instanceFact = config.InstanceFactory;
      std::unique_ptr<CustomisedInstanceFactory> defaultInstanceFact;
      if (instanceFact == nullptr)
        {
          defaultInstanceFact = std::make_unique<CustomisedInstanceFactory> ();
          instanceFact = defaultInstanceFact.get ();
        }

      CHECK (!config.XayaRpcUrl.empty ()) << "XayaRpcUrl must be configured";
      const std::string jsonRpcUrl(config.XayaRpcUrl);
      jsonrpc::HttpClient httpConnector(jsonRpcUrl);

      if (config.XayaRpcWait)
        WaitForXaya (httpConnector);

      auto game = std::make_unique<Game> (gameId);
      game->ConnectRpcClient (httpConnector);
      VerifyXayaVersion (config, game->GetXayaVersion ());
      CHECK (game->DetectZmqEndpoint ());

      const fs::path gameDir = GetGameDirectory (config, gameId,
                                                 game->GetChain ());
      const fs::path dbFile = gameDir / fs::path ("storage.sqlite");

      rules.Initialise (dbFile.u8string ());
      game->SetStorage (rules.GetStorage ());

      game->SetGameLogic (rules);

      if (config.PendingMoves != nullptr)
        game->SetPendingMoveProcessor (*config.PendingMoves);

      if (config.EnablePruning >= 0)
        game->EnablePruning (config.EnablePruning);

      auto components = instanceFact->BuildGameComponents (*game);

      auto serverConnector = CreateRpcServerConnector (config);
      if (serverConnector == nullptr)
          LOG (WARNING)
              << "No connector has been set up for the game RPC server,"
                 " no RPC interface will be available";
      else
          components.push_back (
              instanceFact->BuildRpcServer (*game, *serverConnector));

      for (auto& c : components)
        c->Start ();
      game->Run ();
      for (auto& c : components)
        c->Stop ();

      /* We need to make sure that the Game instance is destructed before the
         storage is.  That is necessary, since destructing the Game instance
         may still cause some batched transactions to be flushed, and this
         needs the storage intact.  */
      game.reset ();
    }
  catch (const std::exception& exc)
    {
      LOG (FATAL) << "Exception caught: " << exc.what ();
    }
  catch (...)
    {
      LOG (FATAL) << "Unknown exception caught";
    }

  return EXIT_SUCCESS;
}

namespace
{

class CallbackGameLogic : public GameLogic
{

private:

  /** The callback pointers.  */
  const GameLogicCallbacks& callbacks;

protected:

  GameStateData
  GetInitialStateInternal (unsigned& height, std::string& hashHex) override
  {
    CHECK (callbacks.GetInitialState != nullptr);
    return callbacks.GetInitialState (GetContext ().GetChain (),
                                      height, hashHex);
  }

  GameStateData
  ProcessForwardInternal (const GameStateData& oldState,
                          const Json::Value& blockData,
                          UndoData& undoData) override
  {
    CHECK (callbacks.ProcessForward != nullptr);
    return callbacks.ProcessForward (GetContext ().GetChain (),
                                     oldState, blockData, undoData);
  }

  GameStateData
  ProcessBackwardsInternal (const GameStateData& oldState,
                            const Json::Value& blockData,
                            const UndoData& undoData) override
  {
    CHECK (callbacks.ProcessBackwards != nullptr);
    return callbacks.ProcessBackwards (GetContext ().GetChain (),
                                       oldState, blockData, undoData);
  }

public:

  explicit CallbackGameLogic (const GameLogicCallbacks& cb)
    : callbacks(cb)
  {}

  Json::Value
  GameStateToJson (const GameStateData& state) override
  {
    if (callbacks.GameStateToJson != nullptr)
      return callbacks.GameStateToJson (state);
    return GameLogic::GameStateToJson (state);
  }

};

} // anonymous namespace

int
DefaultMain (const GameDaemonConfiguration& config, const std::string& gameId,
             const GameLogicCallbacks& callbacks)
{
  CallbackGameLogic rules(callbacks);
  return DefaultMain (config, gameId, rules);
}

} // namespace xaya
