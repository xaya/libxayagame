// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_GAME_HPP
#define XAYAGAME_GAME_HPP

#include "gamelogic.hpp"
#include "mainloop.hpp"
#include "pruningqueue.hpp"
#include "storage.hpp"
#include "transactionmanager.hpp"
#include "uint256.hpp"
#include "zmqsubscriber.hpp"

#include "rpc-stubs/xayarpcclient.h"

#include <json/json.h>
#include <jsonrpccpp/client.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace xaya
{

/**
 * The main class implementing a game on the Xaya platform.  It handles the
 * ZMQ and RPC communication with the Xaya daemon as well as the RPC interface
 * of the game itself.
 *
 * To implement a game, create a subclass of GameLogic that overrides the pure
 * virtual methods with the actual game logic.  Pass it to a new Game instance
 * and Run() it from the binary's main().
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
   * CATCHING_UP:  We are not at the daemon's current tip, and have requested
   * updates to be sent.  Those are processed based on a particular reqtoken
   * for now to bring us up-to-date.
   *
   * UP_TO_DATE:  As far as is known, we are at the current tip of the daemon.
   * Ordinary ZMQ notifications are processed as they come in for changes
   * to the tip, and we expect them to match the current block hash.
   */
  enum class State
  {
    UNKNOWN = 0,
    PREGENESIS,
    OUT_OF_SYNC,
    CATCHING_UP,
    UP_TO_DATE,
  };

  /** This game's game ID.  */
  const std::string gameId;

  /**
   * Mutex guarding internal state.  This is necessary at least in theory since
   * changes might be made from the ZMQ listener on the ZMQ subscriber's
   * worker thread in addition to the main thread.
   *
   * It is also used as lock for the waitforchange condition variable.
   */
  mutable std::mutex mut;

  /**
   * Condition variable that is signalled whenever the game state is changed
   * (due to attached/detached blocks or the initial state becoming known).
   */
  mutable std::condition_variable cvStateChanged;

  /** The chain type to which the game is connected.  */
  Chain chain = Chain::UNKNOWN;

  /** The game's current state.  */
  State state = State::UNKNOWN;

  /**
   * While the state is PREGENESIS, this holds the block hash of the game's
   * initial state to which we are catching up.  For CATCHING_UP, this is
   * the TOBLOCK returned from game_sendupdates.
   *
   * This is compared against the CHILD hashes of block-attach notifications
   * to know when we've finished catching up to the current target.
   */
  uint256 targetBlockHash;

  /**
   * The reqtoken value for the currently processed game_sendupdates request
   * (if the state is CATCHING_UP).
   */
  std::string reqToken;

  /** The JSON-RPC client connection to the Xaya daemon.  */
  std::unique_ptr<XayaRpcClient> rpcClient;

  /** The ZMQ subscriber.  */
  internal::ZmqSubscriber zmq;

  /** Storage system in use.  */
  StorageInterface* storage = nullptr;

  /** The game rules in use.  */
  GameLogic* rules = nullptr;

  /**
   * Desired size for batches of atomic transactions while the game is
   * catching up.  <= 1 means no batching even in these situations.
   */
  unsigned transactionBatchSize = 1000;

  /** The manager for batched atomic transactions.  */
  internal::TransactionManager transactionManager;

  /** The main loop.  */
  internal::MainLoop mainLoop;

  /** The pruning queue if we are pruning.  */
  std::unique_ptr<internal::PruningQueue> pruningQueue;

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
   * Checks whether a ZMQ notification is relevant to the current state,
   * given its (lack of) reqtoken.
   */
  bool IsReqtokenRelevant (const Json::Value& data) const;

  /**
   * Updates the current game state for an attached block.  This does the main
   * work for BlockAttach, after the latter verified the current state, the
   * reqtoken and other higher-level stuff.
   *
   * Returns false if the block cannot be attached directly, and a reinit
   * of the current state is required.
   */
  bool UpdateStateForAttach (const uint256& parent, const uint256& child,
                             const Json::Value& blockData);

  /**
   * Updates the current game state for a detached block.  This does the main
   * work for BlockDetach, after the latter handled the state and reqtoken.
   *
   * Returns false if the detached block does not correspond to the current
   * game state and we need to reinitialise.
   */
  bool UpdateStateForDetach (const uint256& parent, const uint256& child,
                             const Json::Value& blockData);

  /**
   * Starts to sync from the current game state to the current chain tip.
   * This is a helper method called from ReinitialiseState when the state
   * was set to OUT_OF_SYNC.  It checks the current block hash against the
   * best known one from the daemon and then either sets the state to
   * CATCHING_UP and requests game_sendupdates, or sets the state to
   * UP_TO_DATE if all is already fine.
   */
  void SyncFromCurrentState (const Json::Value& blockchainInfo,
                             const uint256& currentHash);

  /**
   * Re-initialises the current game state.  This is called whenever we are not
   * sure, like when ZMQ notifications have been missed or during start up.
   * It checks the storage for the current game state and queries the RPC
   * daemon with getblockchaininfo and then determines what needs to be done.
   */
  void ReinitialiseState ();

  /**
   * Notifies potentially-waiting threads that the state has changed.  Callers
   * must hold the mut lock.
   */
  void NotifyStateChange () const;

  /**
   * Converts a state enum value to a string for use in log messages and the
   * JSON-RPC interface.
   */
  static std::string StateToString (State s);

  friend class GameTestFixture;

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
   * Returns the chain (network) type as enum of the
   * connected Xaya daemon.  This can be used to set up the storage database
   * correctly, for instance.  Must not be called before ConnectRpcClient.
   */
  Chain GetChain () const;

  /**
   * Sets the storage interface to use.  This must be called before starting
   * the main loop, and may not be called while it is running.
   *
   * Important:  The storage instance associated to the Game here needs to
   * remain valid until after the Game instance has been destructed!  This is
   * so that potentially batched transactions can still be flushed.
   */
  void SetStorage (StorageInterface* s);

  /**
   * Sets the game rules to use.  This must be called before starting
   * the main loop, and may not be called while it is running.
   */
  void SetGameLogic (GameLogic* gl);

  /**
   * Enables (or changes) pruning with the given number of blocks to keep.
   * Must be called after the storage is set already.
   */
  void EnablePruning (unsigned nBlocks);

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
   * Requests the server to stop; this may be called always, but only has
   * an effect if the Run() is currently blocking in the main loop.  This method
   * is mainly meant to be exposed by the game daemon through its JSON-RPC
   * interface.
   */
  void
  RequestStop ()
  {
    mainLoop.Stop ();
  }

  /**
   * Returns a JSON object that contains information about the current
   * syncing state, some meta information (game ID, chain) and custom
   * information extracted by a callback function from the current
   * game state.  That data is placed at a custom field in the returned
   * JSON object.
   *
   * This function can be used to implement custom "getter" RPC methods
   * that do not need to return the full game state but just some part
   * of it that is interesting at the moment.
   */
  Json::Value GetCustomStateData (
      const std::string& jsonField,
      const std::function<Json::Value (const GameStateData&)>& cb) const;

  /**
   * Returns a JSON object that contains the current game state as well as
   * some meta information (like the state of the game instance itself
   * and the block the returned state corresponds to).
   *
   * This method is exposed by GameRpcServer externally (as the main external
   * interface to a game daemon), and can be exposed by custom JSON-RPC servers
   * as well.
   */
  Json::Value GetCurrentJsonState () const;

  /**
   * Blocks the calling thread until a change to the game state has
   * (potentially) been made.  This can be used to implement long-polling
   * RPC methods, e.g. for front-ends.  Note that this function may return
   * spuriously in situations when there is no new state.
   *
   * If a non-null pointer is passed in, then the new current block is
   * returned in it.  This is set to null if there is not yet any known
   * state associated to a block (during initial sync).
   *
   * After this function returns, clients will likely want to check if the
   * new current state matches what they already have.  If not, they should
   * use e.g. GetCurrentJsonState to look up the new state and rect to it.
   *
   * This function should only be called when the ZMQ subscriber is running.
   * Otherwise, it will simply return immediately, as there are no changes
   * expected anyway.
   */
  void WaitForChange (uint256* currentBlock = nullptr) const;

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
