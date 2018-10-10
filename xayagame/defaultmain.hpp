// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_DEFAULTMAIN_HPP
#define XAYAGAME_DEFAULTMAIN_HPP

#include "gamelogic.hpp"

#include <string>

namespace xaya
{

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
   * The RPC port at which the game daemon's own JSON-RPC server should be
   * started.  If zero (the default), no server is started.
   */
  int GameRpcPort = 0;

  /**
   * If non-negative (including zero), pruning of old undo data is enabled.
   * The specified value determines how many of the latest blocks are
   * kept to perform reorgs.
   */
  int EnablePruning = -1;

  /**
   * The storage type to be used.  Can be "memory" (default) or "sqlite".
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

} // namespace xaya

#endif // XAYAGAME_DEFAULTMAIN_HPP
