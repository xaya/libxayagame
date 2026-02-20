// Copyright (C) 2018-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_GAMELOGIC_HPP
#define XAYAGAME_GAMELOGIC_HPP

#include "coprocessor.hpp"
#include "storage.hpp"

#include "rpc-stubs/xayarpcclient.h"

#include <xayautil/random.hpp>
#include <xayautil/uint256.hpp>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <json/json.h>

#include <memory>
#include <string>

namespace xaya
{

class GameLogic;

/**
 * The possible chains on which a game can be on the Xaya platform.
 */
enum class Chain
{
  UNKNOWN = 0,

  /* Chains based on Xaya Core */
  MAIN,
  TEST,
  REGTEST,

  /* Polygon network chains */
  POLYGON,
  MUMBAI,

  /* Ganache for EVM-based regtests */
  GANACHE,
};

/**
 * Converts a chain enum value to a string, to be used for printing
 * messages or (for instance) for setting the data directory based
 * on the chain.
 */
std::string ChainToString (Chain c);

/**
 * Converts a string name of a chain to the enum value.  Returns UNKNOWN
 * if the string value does not match any of the expected values.
 */
Chain ChainFromString (const std::string& name);

/**
 * Settings for the JSON-RPC connection to Xaya and a utility class to provide
 * an RPC client based on them.
 */
class XayaRpcProvider
{

private:

  /** The connection URL (we always use HTTP).  */
  std::string url;

  /** The protocol version to use.  */
  jsonrpc::clientVersion_t version;

  /**
   * The data about each thread's client that is held in thread-local storage.
   */
  struct PerThreadData
  {

    /** The underlying HTTP client.  */
    std::unique_ptr<jsonrpc::HttpClient> httpClient;

    /** The actual RPC client.  */
    std::unique_ptr<XayaRpcClient> rpcClient;

  };

  /**
   * Helper function that returns the current thread's data.
   */
  PerThreadData& GetThreadLocalData () const;

public:

  XayaRpcProvider () = default;
  ~XayaRpcProvider ();

  /**
   * Sets the connection settings.  Can only be done once and the settings
   * cannot be changed afterwards (so that existing thread-local clients
   * remain valid).
   */
  void Set (const std::string& u, jsonrpc::clientVersion_t v);

  /**
   * Checks if the settings are provided.
   */
  operator bool () const
  {
    return !url.empty ();
  }

  /**
   * Returns the RPC client.  Must only be called if the settings are provided.
   * This returns a thread-local instance, so that it is safe to use this
   * from multiple threads and use the returned RPC clients in parallel.
   */
  XayaRpcClient& operator* () const;

};

/**
 * Generic class for a processor game state, which mainly holds some contextual
 * information (like the chain and game ID).  This is used as a common
 * superclass for the block update logic (GameLogic) and the logic
 * for processing pending moves (PendingMoveProcessor).
 */
class GameProcessorWithContext
{

private:

  /**
   * The chain that the game is running on.  This may influence the rules
   * and is provided via the Context.
   */
  Chain chain = Chain::UNKNOWN;

  /**
   * The game id of the connected game.  This is used to seed the random
   * number generator.
   */
  std::string gameId;

  /** The RPC settings to use.  */
  const XayaRpcProvider* rpcProvider = nullptr;

protected:

  GameProcessorWithContext () = default;

  /**
   * Returns the chain the game is running on.
   */
  Chain GetChain () const;

  /**
   * Returns the current game ID.
   */
  const std::string& GetGameId () const;

  /**
   * Returns the configured RPC connection to Xaya Core.  Must only be called
   * after InitialiseGameContext was invoked with a non-null RPC client.
   */
  XayaRpcClient& GetXayaRpc ();

public:

  virtual ~GameProcessorWithContext () = default;

  /**
   * Initialises the instance with some data that is obtained by a Game
   * instance after the RPC connection to Xaya Core is up.
   *
   * The RPC settings may be null, but then certain features
   * (depending on GetXayaRpc) will be disabled.
   *
   * This must only be called once.  It is typically done by the Game
   * instance, but may also be used for testing.
   */
  void InitialiseGameContext (Chain c, const std::string& id,
                              const XayaRpcProvider* rpc);

};

/**
 * Context for a call to the callbacks of the GameLogic class.  This is
 * provided by GameLogic itself so that the implementing subclasses can
 * access certain additional information.
 */
class Context
{

private:

  /**
   * Reference to the GameLogic instance.  This is used to access some static
   * data there, like the chain or game ID.
   */
  const GameLogic& logic;

  /** Random-number generator for the current block.  */
  Random rnd;

  /**
   * Coprocessor batch that the implementation can use to access individual
   * coprocessors by name.
   */
  CoprocessorBatch::Block* coprocBlk;

  /**
   * Constructs a context.  This is done by the GameLogic class.
   */
  Context (const GameLogic& l, const uint256& rndSeed,
           CoprocessorBatch::Block* cb);

  friend class GameLogic;

public:

  Context () = delete;
  Context (const Context&) = delete;
  void operator= (const Context&) = delete;

  /**
   * Returns the chain that the game is running on.  Where possible, this
   * should be accessed through Context.  But in some situations there
   * is no context (e.g. SQLiteGame::GetInitialStateBlock), but the chain
   * might still be important.
   */
  Chain GetChain () const;

  /**
   * Returns the game ID of the running game instance.
   */
  const std::string& GetGameId () const;

  /**
   * Returns a reference to a random-number generator that is seeded
   * specifically for the current context (initial-state computation
   * or a particular block that is being attached / detached).
   */
  Random&
  GetRandom ()
  {
    return rnd;
  }

  /**
   * Returns the coprocessor with the given name, casted to the given
   * type.  Returns null if no such coprocessor is registered.
   */
  template <typename T>
    T*
    GetCoprocessor (const std::string& name)
  {
    if (coprocBlk == nullptr)
      return nullptr;
    return coprocBlk->Get<T> (name);
  }

};

/**
 * The interface for actual games.  Implementing classes define the rules
 * of an actual game so that it can be plugged into libxayagame to form
 * a complete game engine.
 *
 * If it is not easily possible to keep the entire state in memory as a
 * GameStateData object, the game may keep the full state data in some
 * external data structure (e.g. an SQLite database) and just return
 * some handle (e.g. the block hash) as GameStateData.  The ProcessForward
 * and ProcessBackwards functions are then responsible for updating the
 * external data structure accordingly.
 *
 * To make sure that changes to the externally-kept game state are consistent
 * with the state that libxayagame keeps, games should leverage the transactions
 * mechanism present in StorageInterface.  For instance, they can define a
 * custom storage implementation that keeps both the external game-state data
 * and the libxayagame-stored data, and allows atomic transactions spanning
 * both of them.
 */
class GameLogic : public GameProcessorWithContext
{

private:

  class ContextSetter;

  /** Current Context instance if any.  */
  Context* ctx = nullptr;

  friend class Context;

protected:

  /**
   * Returns the current Context instance.  This function must only be
   * called while one of the Internal callbacks is running in a subclass.
   */
  Context& GetContext ();

  /**
   * Returns a read-only version of the Context.  This can be used in case
   * some callback functions are marked as const.
   */
  const Context& GetContext () const;

  /**
   * Returns the initial state (as well as the associated block height
   * and block hash in big-endian hex) for the game.  The returned hashHex
   * may be the empty string, in which case only the genesis height is
   * specified and any block hash at that height is accepted.  This is useful
   * e.g. for testing chains that don't have a fixed genesis hash.
   */
  virtual GameStateData GetInitialStateInternal (unsigned& height,
                                                 std::string& hashHex) = 0;

  /**
   * Processes the game logic forward in time:  From an old state and moves
   * (actually, the JSON data sent for block attaches; it includes the moves
   * but also other things like the rngseed), the new state has to be computed.
   *
   * The passed in oldState is either an initial state as returned by
   * GetInitialState (if neither ProcessForward nor ProcessBackwards have been
   * called yet), or the last state returned from ProcessForward
   * or ProcessBackwards.
   */
  virtual GameStateData ProcessForwardInternal (const GameStateData& oldState,
                                                const Json::Value& blockData,
                                                UndoData& undoData) = 0;

  /**
   * Processes the game logic backwards in time:  Compute the previous
   * game state from the "new" one, the moves and the undo data.
   *
   * The passed in newState is the state that was returned by the last
   * call to ProcessForward or ProcessBackwards.
   */
  virtual GameStateData ProcessBackwardsInternal (const GameStateData& newState,
                                                  const Json::Value& blockData,
                                                  const UndoData& undoData) = 0;

public:

  GameLogic () = default;

  /**
   * Returns the initial state for the game.  This is the function that is
   * called externally.  It sets up a Context instance and then calls
   * through to GetInitialStateInternal.  The coprocessor batch is optional,
   * and will not be set when this method is called to determine the
   * genesis height initially.
   */
  GameStateData GetInitialState (unsigned& height, std::string& hashHex,
                                 CoprocessorBatch::Block* cb);

  /**
   * Processes the game state forward in time.  This method should be
   * called externally for this.  It sets up a Context instance and then
   * delegates the actual work to ProcessForwardInternal.
   */
  GameStateData ProcessForward (const GameStateData& oldState,
                                const Json::Value& blockData,
                                UndoData& undoData,
                                CoprocessorBatch::Block* cb);

  /**
   * Processes the game state backwards in time (for reorgs).  This function
   * should be called externally.  It handles the Context setup and then
   * does the actual work through ProcessBackwardsInternal.
   */
  GameStateData ProcessBackwards (const GameStateData& newState,
                                  const Json::Value& blockData,
                                  const UndoData& undoData,
                                  CoprocessorBatch::Block* cb);

  /**
   * A notification method that gets called whenever the Game instance
   * updated the game state, when the new state has been committed to storage.
   * The blockData will contain the main block information to which the
   * new state corresponds, like "height" and "hash".
   */
  virtual void
  GameStateUpdated (const GameStateData& state, const Json::Value& blockData)
  {}

  /**
   * A notification method that gets called whenever the instance
   * state (not necessarily the game state) changes.  This could be changes
   * to the game state, but also things like losing connection to the
   * blockchain node, reaching the target block and things like that.
   *
   * The argument passed is a basic representation of the instance
   * state as returned also from GetCustomStateData, in particular with the
   * gameid, chain and state fields.
   */
  virtual void
  InstanceStateChanged (const Json::Value& newState)
  {}

  /**
   * Converts an encoded game state to JSON format, which can be returned as
   * game state through the external JSON-RPC interface.  The default
   * implementation is to just return the raw GameStateData as string.
   *
   * The state passed to this function is either the initial state returned
   * by GetInitialState if neither ProcessForward or ProcessBackwards have
   * been called yet, or the result of the last call to either of those
   * two functions.
   */
  virtual Json::Value GameStateToJson (const GameStateData& state);

};

/**
 * Subclass of GameLogic that can be used for games whose state is small enough
 * so that it can be used as "undo data" itself (ideally together with pruning).
 * This allows games to be implemented without undo logic, and may be the
 * best and easiest solution for very simple games.
 */
class CachingGame : public GameLogic
{

protected:

  /**
   * Processes the game logic forward in time, but does not produce any
   * undo data.  This function needs to be implemented by concrete games
   * instead of ProcessForward and ProcessBackwards of GameLogic.
   */
  virtual GameStateData UpdateState (const GameStateData& oldState,
                                     const Json::Value& blockData) = 0;

  GameStateData ProcessForwardInternal (const GameStateData& oldState,
                                        const Json::Value& blockData,
                                        UndoData& undoData) override;
  GameStateData ProcessBackwardsInternal (const GameStateData& newState,
                                          const Json::Value& blockData,
                                          const UndoData& undoData) override;

};

} // namespace xaya

#endif // XAYAGAME_GAMELOGIC_HPP
