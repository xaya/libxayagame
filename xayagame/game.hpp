// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_GAME_HPP
#define XAYAGAME_GAME_HPP

#include "mainloop.hpp"
#include "storage.hpp"
#include "uint256.hpp"
#include "zmqsubscriber.hpp"

#include "rpc-stubs/xayarpcclient.h"

#include <json/json.h>
#include <jsonrpccpp/client.h>

#include <memory>
#include <mutex>
#include <string>

namespace xaya
{

/**
 * The interface for actual games.  Implementing classes define the rules
 * of an actual game so that it can be plugged into libxayagame to form
 * a complete game engine.
 */
class GameLogic
{

private:

  /**
   * The chain ("main", "test" or "regtest") that the game is running on.
   * This may influence the rules and is provided via GetChain.
   */
  std::string chain;

protected:

  /**
   * Returns the chain the game is running on.
   */
  const std::string& GetChain () const;

public:

  GameLogic () = default;
  virtual ~GameLogic () = default;

  /**
   * Sets the chain value.  This is typically called by the Game instance,
   * but may be used also for unit testing.
   *
   * If the chain was already set, it must not be changed to a different value
   * during the lifetime of the object.
   */
  void SetChain (const std::string& c);

  /**
   * Returns the initial state (as well as the associated block height
   * and block hash in big-endian hex) for the game.
   */
  virtual GameStateData GetInitialState (unsigned& height,
                                         std::string& hashHex) = 0;

  /**
   * Processes the game logic forward in time:  From an old state and moves
   * (actually, the JSON data sent for block attaches; it includes the moves
   * but also other things like the rngseed), the new state has to be computed.
   */
  virtual GameStateData ProcessForward (const GameStateData& oldState,
                                        const Json::Value& blockData,
                                        UndoData& undoData) = 0;

  /**
   * Processes the game logic backwards in time:  Compute the previous
   * game state from the "new" one, the moves and the undo data.
   */
  virtual GameStateData ProcessBackwards (const GameStateData& newState,
                                          const Json::Value& blockData,
                                          const UndoData& undoData) = 0;

};

/**
 * The main class implementing a game on the Xaya platform.  It handles the
 * ZMQ and RPC communication with the Xaya daemon as well as the RPC interface
 * of the game itself.
 *
 * To implement a game, create a subclass that overrides the pure virtual
 * methods with the actual game logic and then instantiate and Run() it
 * from the binary's main().
 */
class Game : private internal::ZmqListener
{

private:

  /**
   * States for the game engine during syncing / operation.  The basic states
   * and transitions between states are as follows:
   *
   * UNKNOWN:  The state is currently not known / well-defined.  This is the
   * case initially before the main loop is started and also briefly whenever
   * a ZMQ message is missed and we re-initialise.  Except for these situations,
   * this state should not occur.
   *
   * PREGENESIS:  The core daemon is currently (or when the state was last
   * checked) synced to a block height below the initial state provided by
   * the GameLogic.  There is no current game state, and we wait until the
   * core daemon reaches the game's "genesis" block -- at that point, the
   * initial game state will be written as current and the state is changed
   * to OUT_OF_SYNC.
   *
   * OUT_OF_SYNC:  We have a current game state, but it is not (necessarily)
   * the current blockchain tip in the daemon.  This state occurs only briefly,
   * and is changed to CATCHING_UP when a game_sendupdates request has been
   * sent to bring the game state up to the tip.
   *
   * TODO: More states when we go beyond just getting the initial state into
   * the system.
   */
  enum class State
  {
    UNKNOWN = 0,
    PREGENESIS,
    OUT_OF_SYNC,
  };

  /** This game's game ID.  */
  const std::string gameId;

  /**
   * Mutex guarding internal state.  This is necessary at least in theory since
   * changes might be made from the ZMQ listener on the ZMQ subscriber's
   * worker thread in addition to the main thread.
   */
  mutable std::mutex mut;

  /** The chain type (main, test, regtest) to which the game is connected.  */
  std::string chain;

  /** The game's current state.  */
  State state = State::UNKNOWN;

  /**
   * While the state is PREGENESIS, this holds the block hash of the game's
   * initial state to which we are catching up.  This is compared against
   * the CHILD hashes of block-attach notifications to know when that is
   * the case.
   */
  uint256 gameGenesisHash;

  /** The JSON-RPC client connection to the Xaya daemon.  */
  std::unique_ptr<XayaRpcClient> rpcClient;

  /** The ZMQ subscriber.  */
  internal::ZmqSubscriber zmq;

  /** Storage system in use.  */
  StorageInterface* storage = nullptr;

  /** The game rules in use.  */
  GameLogic* rules = nullptr;

  /** The main loop.  */
  internal::MainLoop mainLoop;

  /**
   * The JSON-RPC version to use for talking to Xaya Core.  The actual daemon
   * needs V1, but for the unit test (where the server is mocked and set up
   * based on jsonrpccpp), we want V2.
   */
  static jsonrpc::clientVersion_t rpcClientVersion;

  void BlockAttach (const std::string& id, const Json::Value& data,
                    bool seqMismatch) override;
  void BlockDetach (const std::string& id, const Json::Value& data,
                    bool seqMismatch) override;

  /**
   * Adds this game's ID to the tracked games of the core daemon.
   */
  void TrackGame ();

  /**
   * Removes this game's ID from the tracked games of the core daemon.
   */
  void UntrackGame ();

  /**
   * Re-initialises the current game state.  This is called whenever we are not
   * sure, like when ZMQ notifications have been missed or during start up.
   * It checks the storage for the current game state and queries the RPC
   * daemon with getblockchaininfo and then determines what needs to be done.
   */
  void ReinitialiseState ();

  friend class GameTests;

public:

  explicit Game (const std::string& id);

  Game () = delete;
  Game (const Game&) = delete;
  void operator= (const Game&) = delete;

  /**
   * Sets up the RPC client based on the given connector.
   */
  void ConnectRpcClient (jsonrpc::IClientConnector& conn);

  /**
   * Returns the chain (network type, "main", "test" or "regtest") of the
   * connected Xaya daemon.  This can be used to set up the storage database
   * correctly, for instance.  Must not be called before ConnectRpcClient.
   */
  const std::string& GetChain () const;

  /**
   * Sets the storage interface to use.  This must be called before starting
   * the main loop, and may not be called while it is running.
   */
  void SetStorage (StorageInterface* s);

  /**
   * Sets the game rules to use.  This must be called before starting
   * the main loop, and may not be called while it is running.
   */
  void SetGameLogic (GameLogic* gl);

  /**
   * Sets the ZMQ endpoint that will be used to connect to the ZMQ interface
   * of the Xaya daemon.  Must not be called anymore after Start() or
   * Run() have been called.
   */
  void
  SetZmqEndpoint (const std::string& addr)
  {
    zmq.SetEndpoint (addr);
  }

  /**
   * Detects the ZMQ endpoint by calling getzmqnotifications on the Xaya
   * daemon.  Returns false if pubgameblocks is not enabled.
   */
  bool DetectZmqEndpoint ();

  /**
   * Starts the ZMQ subscriber and other logic.  Must not be called before
   * the ZMQ endpoint has been configured, and must not be called when
   * the game is already running.
   */
  void Start ();

  /**
   * Stops the ZMQ subscriber and other logic.  Must only be called if it is
   * currently running.
   */
  void Stop ();

  /**
   * Runs the main event loop for the Game.  This starts the game logic as
   * Start does, blocks the calling thread until a stop of the server is
   * requested, and then stops everything again.
   */
  void Run ();

};

} // namespace xaya

#endif // XAYAGAME_GAME_HPP
