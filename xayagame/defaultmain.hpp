// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_DEFAULTMAIN_HPP
#define XAYAGAME_DEFAULTMAIN_HPP

#include "game.hpp"
#include "gamelogic.hpp"
#include "pendingmoves.hpp"
#include "sqlitegame.hpp"
#include "storage.hpp"

#include <jsonrpccpp/server/connectors/httpserver.h>

#include <json/json.h>

#include <memory>
#include <string>
#include <vector>

namespace xaya
{

/**
 * Possible choices for the game's JSON-RPC server that should be
 * started by the main function.
 */
enum class RpcServerType
{
  /** Do not start any JSON-RPC server.  */
  NONE = 0,
  /** Start a JSON-RPC server listening through HTTP.  */
  HTTP = 1,
};

/**
 * Interface for a general component of the game daemon that runs while
 * the game is running.  For instance, the API RPC server, or another API.
 */
class GameComponent
{

protected:

  GameComponent () = default;

public:

  virtual ~GameComponent () = default;

  GameComponent (const GameComponent&) = delete;
  void operator=  (const GameComponent&) = delete;

  /**
   * Starts the component (when the game is set up).
   */
  virtual void Start () = 0;

  /**
   * Stops the component after the game is stopped.
   */
  virtual void Stop () = 0;

};

/**
 * Interace for a class that runs the game's RPC server.  We need this mainly
 * since the server classes from jsonrpccpp do not inherit from a single
 * super class which we could use instead.
 */
using RpcServerInterface = GameComponent;

/**
 * Simple implementation of GameComponent, that simply wraps a templated
 * libjson-rpc-cpp RPC server.
 */
template <typename T>
  class WrappedRpcServer : public RpcServerInterface
{

private:

  T server;

public:

  template <typename... Args>
    WrappedRpcServer (Args&... args)
    : server(args...)
  {}

  T&
  Get ()
  {
    return server;
  }

  const T&
  Get () const
  {
    return server;
  }

  void
  Start () override
  {
    server.StartListening ();
  }

  void
  Stop () override
  {
    server.StopListening ();
  }

};

/**
 * Factory interface for constructing instances of classes that are not
 * required to be provided (like the GameLogic), but can optionally be
 * customised by games.  An example of such a class is the RPC server.
 */
class CustomisedInstanceFactory
{

public:

  CustomisedInstanceFactory () = default;
  virtual ~CustomisedInstanceFactory () = default;

  CustomisedInstanceFactory (const CustomisedInstanceFactory&) = delete;
  void operator= (const CustomisedInstanceFactory&) = delete;

  /**
   * Returns an instance of the RPC server that should be used for the game.
   * By default, this method builds a standard GameRpcServer.
   */
  virtual std::unique_ptr<RpcServerInterface> BuildRpcServer (
      Game& game,
      jsonrpc::AbstractServerConnector& conn);

  /**
   * Builds general components that should be run alongside the game.
   * By default, just returns an empty list.
   */
  virtual std::vector<std::unique_ptr<GameComponent>> BuildGameComponents (
      Game& game);

};

/**
 * Basic configuration parameters for running a game daemon.  This corresponds
 * to the default command-line flags, but allows to set them programmatically
 * from a context where actual dependence on gflags would not be possible.
 */
struct GameDaemonConfiguration
{

  /**
   * The URL at which Xaya Core's JSON-RPC interface is available.  This
   * should already include the credentials, as in:
   *
   *  http://user:password@localhost:port
   */
  std::string XayaRpcUrl;

  /**
   * The minimum required Xaya Core version.  By default, this is
   * Xaya Core 1.1.2, since when the extended verifymessage command is available
   * (which is used by GameLogic).
   */
  unsigned MinXayaVersion = 1010200;

  /**
   * The maximum possible Xaya Core version.  If zero (the default), then
   * no maximum version is imposed.  This can be set for instance if
   * incompatible changes to the ZMQ interface are made in the future, and
   * the old game daemon needs to be updated to work with newer Xaya Core.
   */
  unsigned MaxXayaVersion = 0;

  /**
   * The type of JSON-RPC server that should be started for the game
   * (if any).
   */
  RpcServerType GameRpcServer = RpcServerType::NONE;

  /**
   * The RPC port at which the game daemon's own JSON-RPC server should be
   * started.  This must be set if GameRpcServer is set to HTTP or TCP.
   */
  int GameRpcPort = 0;

  /**
   * Whether or not the game daemon's JSON-RPC server shoud listen only locally
   * (if set to true / left to the default) or on all interfaces (if set to
   * false explicitly).
   */
  bool GameRpcListenLocally = true;

  /**
   * If non-negative (including zero), pruning of old undo data is enabled.
   * The specified value determines how many of the latest blocks are
   * kept to perform reorgs.
   */
  int EnablePruning = -1;

  /**
   * The storage type to be used.  Can be "memory" (default), "lmdb"
   * or "sqlite".
   */
  std::string StorageType = "memory";

  /**
   * The base data directory for persistent storage.  Must be set unless memory
   * storage is selected.  The game ID is added as an additional directory part
   * to it, so that multiple libxayagame-based games can be in the same base
   * data directory.
   */
  std::string DataDirectory;

  /**
   * If set to non-null, then this PendingMoveProcessor instance is associated
   * to the Game.
   */
  PendingMoveProcessor* PendingMoves = nullptr;

  /**
   * Factory class for customed instances of certain optional classes
   * like the RPC server.  If not set, default classes are used instead.
   *
   * The pointer here is just a pointer and not owned by the struct.  It must
   * be managed outside of the DefaultMain call.
   */
  CustomisedInstanceFactory* InstanceFactory = nullptr;

};

/**
 * Runs a default "main" function for Xaya game daemons.  This default main
 * accepts a few configuration options through GameDaemonConfiguration and
 * starts up a game daemon waiting loop.
 *
 * This can be used to create game daemons even simpler than with libxayagame
 * itself for cases where no custom configuration / setup is required.  The
 * real main function only needs to instantiate an appropriate GameLogic
 * instance and pass this together with desired configuration flags to
 * this default main.
 */
int DefaultMain (const GameDaemonConfiguration& config,
                 const std::string& gameId,
                 GameLogic& rules);

/**
 * Runs a default main function for SQLite-based Xaya game daemons.  The
 * details of the started game daemon can be configured through the config
 * struct's values.
 *
 * Note that this function always ignores config.StorageType and instead
 * uses "sqlite".
 */
int SQLiteMain (const GameDaemonConfiguration& config,
                const std::string& gameId,
                SQLiteGame& rules);

/**
 * Struct that holds function pointers for implementations of the
 * various GameLogic functions.  This can be passed directly to the
 * below DefaultMain variant, which allows writing game rules without
 * actually subclassing GameLogic.
 *
 * All fields here match the function of the same name in GameLogic.
 */
struct GameLogicCallbacks
{

  /* The following functions are mandatory and must be explicitly set
     to a non-null value.  */
  GameStateData (*GetInitialState) (Chain chain, unsigned& height,
                                    std::string& hashHex) = nullptr;
  GameStateData (*ProcessForward) (Chain chain, const GameStateData& oldState,
                                   const Json::Value& blockData,
                                   UndoData& undoData) = nullptr;
  GameStateData (*ProcessBackwards) (Chain chain, const GameStateData& newState,
                                     const Json::Value& blockData,
                                     const UndoData& undoData) = nullptr;

  /* These functions are optional and can be kept as null, in which case
     their default implementation from GameLogic will be used.  */
  Json::Value (*GameStateToJson) (const GameStateData& state) = nullptr;

};

/**
 * Runs a DefaultMain, but with callbacks that specify the game rules instead
 * of a GameLogic instance.  This makes it possible to build games without
 * even subclassing GameLogic, simply by providing two functions that handle
 * the game logic.
 */
int DefaultMain (const GameDaemonConfiguration& config,
                 const std::string& gameId,
                 const GameLogicCallbacks& callbacks);

} // namespace xaya

#endif // XAYAGAME_DEFAULTMAIN_HPP
