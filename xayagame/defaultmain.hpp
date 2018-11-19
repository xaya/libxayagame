// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_DEFAULTMAIN_HPP
#define XAYAGAME_DEFAULTMAIN_HPP

#include "gamelogic.hpp"
#include "storage.hpp"

#include <json/json.h>

#include <string>

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
  /** Start a JSON-RPC server listening through a plain TCP socket.  */
  TCP = 2,
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
