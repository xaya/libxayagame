// Copyright (C) 2018-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_GAME_HPP
#define XAYAGAME_GAME_HPP

#include "gamelogic.hpp"
#include "heightcache.hpp"
#include "mainloop.hpp"
#include "pendingmoves.hpp"
#include "pruningqueue.hpp"
#include "storage.hpp"
#include "transactionmanager.hpp"
#include "zmqsubscriber.hpp"

#include "rpc-stubs/xayarpcclient.h"

#include <xayautil/uint256.hpp>

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

  /**
   * Condition variable that is signalled whenever the pending state
   * is changed.
   */
  mutable std::condition_variable cvPendingStateChanged;

  /** The chain type to which the game is connected.  */
  Chain chain = Chain::UNKNOWN;

  /** The game's current state.  */
  State state = State::UNKNOWN;

  /**
   * The game's genesis height, if known already.  We cache that from the
   * first call to GetInitialState, so that we can avoid calling it all
   * over again on each block before we reach the genesis height.
   */
  int genesisHeight;
  /**
   * The game's genesis hash, if already known (zero otherwise).  Games may
   * specify only a genesisHeight and no hash, in which case the hash will be
   * zero until we leave PREGENESIS phase and get the actual hash for the
   * block height from the blockchain daemon.
   */
  uint256 genesisHash;

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

  /** The height-caching storage we use.  */
  std::unique_ptr<internal::StorageWithCachedHeight> storage;

  /** The game rules in use.  */
  GameLogic* rules = nullptr;

  /** The processor for pending moves, if any.  */
  PendingMoveProcessor* pending = nullptr;

  /**
   * Version number of the "current" pending state.  This number is incremented
   * whenever the pending state may have changed, and is used to identify
   * known states with WaitForPendingChange.
   */
  int pendingStateVersion = 1;

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

  void BlockAttach (const std::string& id, const Json::Value& data,
                    bool seqMismatch) override;
  void BlockDetach (const std::string& id, const Json::Value& data,
                    bool seqMismatch) override;
  void PendingMove (const std::string& id, const Json::Value& data) override;

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
   * Notifies potentially-waiting threads that the pending state has changed.
   */
  void NotifyPendingStateChange ();

  /**
   * Returns the current pending state as JSON, but assuming that the caller
   * already holds the mut lock.
   */
  Json::Value UnlockedPendingJsonState () const;

  /**
   * Converts a state enum value to a string for use in log messages and the
   * JSON-RPC interface.
   */
  static std::string StateToString (State s);

  friend class GameTestFixture;

public:

  /**
   * Special value for the old version in WaitForPendingChange that tells the
   * function to always block.
   */
  static constexpr int WAITFORCHANGE_ALWAYS_BLOCK = 0;

  /**
   * Callback function that retrieves custom state JSON from a game state
   * and that requires a lock on the Game instance.
   */
  using ExtractJsonFromStateWithLock
    = std::function<Json::Value (const GameStateData& state,
                                 const uint256& hash, unsigned height,
                                 std::unique_lock<std::mutex> lock)>;

  /**
   * Callback function that retrieves some custom state JSON from
   * a game state with block height information.
   */
  using ExtractJsonFromStateWithBlock
    = std::function<Json::Value (const GameStateData& state,
                                 const uint256& hash, unsigned height)>;

  /**
   * Callback function that retrieves some custom state JSON from
   * a game state alone.
   */
  using ExtractJsonFromState
    = std::function<Json::Value (const GameStateData& state)>;

  explicit Game (const std::string& id);

  Game () = delete;
  Game (const Game&) = delete;
  void operator= (const Game&) = delete;

  /**
   * Sets up the RPC client based on the given connector.  This must only
   * be called once.  The JSON-RPC protocol version to use can be specified.
   * V1 is what needs to be used with a real Xaya Core instance, while
   * unit tests and other situations (e.g. Xaya X) need V2.
   */
  void ConnectRpcClient (
      jsonrpc::IClientConnector& conn,
      jsonrpc::clientVersion_t version = jsonrpc::JSONRPC_CLIENT_V1);

  /**
   * Returns the version of the connected Xaya Core daemon in the form
   * AABBCCDD, where AA is the major version, BB the minor version, CC the
   * revision and DD the build.  For instance, 1020300 would be Xaya Core
   * 1.2.3.
   *
   * This method CHECK fails if the RPC connection has not yet been set up.
   */
  unsigned GetXayaVersion () const;

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
  void SetStorage (StorageInterface& s);

  /**
   * Sets the game rules to use.  This must be called before starting
   * the main loop, and may not be called while it is running.
   */
  void SetGameLogic (GameLogic& gl);

  /**
   * Sets the processor for pending moves.  Setting one is optional; if no
   * processor is set of pending move notifications are not enabled in the
   * connected Xaya Core, then no pending state will be available.
   */
  void SetPendingMoveProcessor (PendingMoveProcessor& p);

  /**
   * Enables (or changes) pruning with the given number of blocks to keep.
   * Must be called after the storage is set already.
   */
  void EnablePruning (unsigned nBlocks);

  /**
   * Detects the ZMQ endpoint(s) by calling getzmqnotifications on the Xaya
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
   * syncing state and custom information extracted by a callback from the
   * game state.
   *
   * The callback will be provided with a std::unique_lock on the Game
   * instance, so it has the ability to control the potential for
   * parallel calls (e.g. if it needs to obtain a database snapshot
   * before allowing other threads to modify the instance).
   */
  Json::Value GetCustomStateData (
      const std::string& jsonField,
      const ExtractJsonFromStateWithLock& cb) const;

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
      const ExtractJsonFromStateWithBlock& cb) const;

  /**
   * Extracts custom state JSON as per the other overload, but the callback
   * gets only passed the game state itself.  This is enough for many situations
   * and corresponds to the previous version of this API.
   */
  Json::Value GetCustomStateData (const std::string& jsonField,
                                  const ExtractJsonFromState& cb) const;

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
   * Returns a JSON object that just contains basic stats about the game daemon
   * itself (e.g. syncing state, current block height) but no specific pieces
   * of data about the game state.  This is useful as the cheapest possible
   * way to check on the health and connection state of a game daemon.
   */
  Json::Value GetNullJsonState () const;

  /**
   * Returns a JSON object that contains data about the current state
   * of pending moves as JSON.
   *
   * If no PendingMoveProcessor is attached or if pending moves are disabled
   * in the Xaya Core notifications, then this raises a JSON-RPC error.
   */
  Json::Value GetPendingJsonState () const;

  /**
   * Checks to see if the instance considers itself "healthy", i.e. able
   * to properly serve clients.  This means that it is up-to-date (to its
   * best knowledge).
   */
  bool IsHealthy () const;

  /**
   * Blocks the calling thread until a change to the game state has
   * (potentially) been made.  This can be used to implement long-polling
   * RPC methods, e.g. for front-ends.  Note that this function may return
   * spuriously in situations when there is no new state.
   *
   * When oldBlock is non-null and does not match the best block at the time
   * of entering WaitForChange, then the function returns immediately.  This
   * can be used to prevent race conditions where a new state came in between
   * the last return from WaitForChange and when a client finishes processing
   * that change and calls again.
   *
   * The new new best block is returned after the detected change.  This is set
   * to null if there is not yet any known state associated to a block
   * (during initial sync).
   *
   * After this function returns, clients will likely want to check if the
   * new current state matches what they already have.  If not, they should
   * use e.g. GetCurrentJsonState to look up the new state and rect to it.
   *
   * This function should only be called when the ZMQ subscriber is running.
   * Otherwise, it will simply return immediately, as there are no changes
   * expected anyway.
   */
  void WaitForChange (const uint256& oldBlock, uint256& newBlock) const;

  /**
   * Blocks the calling thread until a change to the pending state has
   * been made.  Note that this function may return spuriously.
   * Returns the new pending state as per GetPendingJsonState.
   *
   * If oldVersion is passed and not WAITFORCHANGE_ALWAYS_BLOCK, then the
   * method returns immediately if the version of the current pending state
   * (as returned in the JSON field "version") does not match the one
   * passed in.
   */
  Json::Value WaitForPendingChange (int oldState) const;

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
