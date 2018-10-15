// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_GAMELOGIC_HPP
#define XAYAGAME_GAMELOGIC_HPP

#include "storage.hpp"

#include <json/json.h>

#include <string>

namespace xaya
{

/**
 * The possible chains on which a game can be in the Xaya network.
 */
enum class Chain
{
  UNKNOWN = 0,
  MAIN = 1,
  TEST = 2,
  REGTEST = 3,
};

/**
 * Converts a chain enum value to a string, to be used for printing
 * messages or (for instance) for setting the data directory based
 * on the chain.
 */
std::string ChainToString (Chain c);

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
 */
class GameLogic
{

private:

  /**
   * The chain that the game is running on.  This may influence the rules
   * and is provided via GetChain.
   */
  Chain chain = Chain::UNKNOWN;

protected:

  /**
   * Returns the chain the game is running on.
   */
  Chain GetChain () const;

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
  void SetChain (Chain c);

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
   *
   * The passed in oldState is either an initial state as returned by
   * GetInitialState (if neither ProcessForward nor ProcessBackwards have been
   * called yet), or the last state returned from ProcessForward
   * or ProcessBackwards.
   */
  virtual GameStateData ProcessForward (const GameStateData& oldState,
                                        const Json::Value& blockData,
                                        UndoData& undoData) = 0;

  /**
   * Processes the game logic backwards in time:  Compute the previous
   * game state from the "new" one, the moves and the undo data.
   *
   * The passed in newState is the state that was returned by the last
   * call to ProcessForward or ProcessBackwards.
   */
  virtual GameStateData ProcessBackwards (const GameStateData& newState,
                                          const Json::Value& blockData,
                                          const UndoData& undoData) = 0;

  /**
   * Tells the game that a change to the game state is about to be made
   * (because a new block is being attached or detached).
   *
   * Transactions will not be nested, i.e. this function is only called when
   * the last transaction has either been committed or rolled back.
   *
   * By default, this function does nothing.  If the game logic keeps track
   * of state in an external data structure, it can use this function together
   * with CommitTransaction and RollbackTransaction to ensure consistency
   * between the state it keeps and the state that libxayagame keeps in its
   * storage.
   */
  virtual void BeginTransaction ();

  /**
   * Tells the game that all state changes related to the previously started
   * transaction have been completed successfully.
   */
  virtual void CommitTransaction ();

  /**
   * Tells the game that there was an error during the state changes for the
   * previously started transaction, and all changes made to internal states
   * since then should be reverted if possible.
   */
  virtual void RollbackTransaction ();

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

public:

  GameStateData ProcessForward (const GameStateData& oldState,
                                const Json::Value& blockData,
                                UndoData& undoData) override;
  GameStateData ProcessBackwards (const GameStateData& newState,
                                  const Json::Value& blockData,
                                  const UndoData& undoData) override;

};

} // namespace xaya

#endif // XAYAGAME_GAMELOGIC_HPP
