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

  /**
   * Converts an encoded game state to JSON format, which can be returned as
   * game state through the external JSON-RPC interface.  The default
   * implementation is to just return the raw GameStateData as string.
   */
  virtual Json::Value GameStateToJson (const GameStateData& state);

};

} // namespace xaya

#endif // XAYAGAME_GAMELOGIC_HPP
