// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_SQLITEGAME_HPP
#define XAYAGAME_SQLITEGAME_HPP

#include "gamelogic.hpp"
#include "storage.hpp"

#include <sqlite3.h>

#include <json/json.h>

#include <memory>
#include <string>

namespace xaya
{

/**
 * Subclass for GameLogic for games that store their state internally
 * in an SQLite database.  They only need to implement the game logic
 * in a single function, namely updating an SQLite database handle for
 * a given block of moves.  Rollbacks and undo data are handled by the
 * SQLiteGame, using the SQLite session extension internally.
 *
 * To ensure consistency and atomic operation between the game's internal
 * data in the database and libxayagame's state, the underlying SQLiteStorage
 * used by SQLiteGame should be used as the main storage in Game (so that it
 * also holds undo data and the current game state).
 *
 * With this GameLogic implementation, the "game state" as seen by libxayagame
 * is simply the keyword string "initial" for the initial state and
 * "block <hash>" with the associated block hash for other states.
 * (The hash is used to counter-check for consistency and make sure that only
 * the current state is ever updated as it is guaranteed by GameLogic.)
 *
 * The undo data for a block is the changeset created by the SQLite session
 * extension for the modifications to the database done by the game itself
 * (but not through the SQLiteStorage, as that is handled by libxayagame).
 */
class SQLiteGame : public GameLogic
{

private:

  /* The subclass of SQLiteStorage that is used internally to manage the
     underlying SQLite database.  The definition and implementation is an
     implementation detail and part of sqlitegame.cpp.  */
  class Storage;

  /**
   * The storage instance that is used.  std::unique_ptr is used so that we
   * can use the forward-declared type and do not need to define Storage right
   * here inline.
   */
  std::unique_ptr<Storage> database;

protected:

  /**
   * This method is called on every open of the SQLite database, and should
   * ensure that the database schema is set up correctly.  It should create it
   * when the database has been created, and may change it if the database
   * was created with an old software version and should be upgraded.  If the
   * schema is already set up correctly, it should do nothing.
   *
   * Subclasses can override the method to do their own set up.
   *
   * Note that table names starting with "xayagame_" are reserved for use by
   * libxayagame itself and must not be used by game implementations.
   */
  virtual void SetupSchema (sqlite3* db);

  /**
   * Returns the height and block hash (as big-endian hex) at which the
   * game's initial state is defined.  The state itself is specified by
   * the implementation of InitialiseState.
   */
  virtual void GetInitialStateBlock (unsigned& height,
                                     std::string& hashHex) const = 0;

  /**
   * Sets the state stored in the database to the initial game state.  It may
   * be assumed that no existing data is stored in the database, except what
   * was potentially inserted through SetupSchema.
   */
  virtual void InitialiseState (sqlite3* db) = 0;

  /**
   * Updates the current state in the database for the given block of moves.
   * Note that no un-finalised sqlite3_stmt handles or other things open
   * against the database may be left behind when the function returns.
   */
  virtual void UpdateState (sqlite3* db, const Json::Value& blockData) = 0;

  /**
   * Retrieves the current state in the database and encodes it as JSON
   * to be returned by the game daemon's JSON-RPC interface.
   */
  virtual Json::Value GetStateAsJson (sqlite3* db) = 0;

public:

  explicit SQLiteGame (const std::string& f);
  virtual ~SQLiteGame ();

  SQLiteGame () = delete;
  SQLiteGame (const SQLiteGame&) = delete;
  void operator= (const SQLiteGame&) = delete;

  /**
   * Returns the storage implementation used internally, which should be set
   * as main storage in Game.
   */
  StorageInterface* GetStorage ();

  GameStateData GetInitialState (unsigned& height,
                                 std::string& hashHex) override;

  GameStateData ProcessForward (const GameStateData& oldState,
                                const Json::Value& blockData,
                                UndoData& undo) override;

  GameStateData ProcessBackwards (const GameStateData& newState,
                                  const Json::Value& blockData,
                                  const UndoData& undo) override;

  void BeginTransaction () override;
  void CommitTransaction () override;
  void RollbackTransaction () override;

  Json::Value GameStateToJson (const GameStateData& state) override;

};

} // namespace xaya

#endif // XAYAGAME_SQLITEGAME_HPP
