// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_SQLITEGAME_HPP
#define XAYAGAME_SQLITEGAME_HPP

#include "game.hpp"
#include "gamelogic.hpp"
#include "storage.hpp"

#include <sqlite3.h>

#include <json/json.h>

#include <functional>
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

  /* Internal helper class (defined and implemented in sqlitegame.cpp) that
     holds a set of currently activate AutoId values.  */
  class ActiveAutoIds;

  /**
   * The storage instance that is used.  std::unique_ptr is used so that we
   * can use the forward-declared type and do not need to define Storage right
   * here inline.
   */
  std::unique_ptr<Storage> database;

  /**
   * Currently active AutoIds (if any).  This pointer is set for use by
   * the Ids() member function while a set is managed (e.g. during a call
   * to UpdateState).  It is set to null when no set is active.
   */
  ActiveAutoIds* activeIds = nullptr;

protected:

  class AutoId;

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

  /**
   * Prepares an SQLite statement in the underlying database and returns
   * the prepared statement.  The returned statement is owned and managed
   * by the SQLiteStorage and must not be freed manually!
   */
  sqlite3_stmt* PrepareStatement (const std::string& sql) const;

  /**
   * Returns a handle to an AutoId instance for a given named key.  That can
   * be used to generate a consistent sequence of integer IDs.
   */
  AutoId& Ids (const std::string& key);

  /**
   * Extracts custom state data from the database (as done by a callback
   * that queries the data).  This calls GetCustomStateData on the Game
   * instance and provides a callback that handles the "game state" string
   * in the same way as GameStateToJson does, before calling the user function
   * to actually retrieve the data.
   */
  Json::Value GetCustomStateData (
      const Game& game, const std::string& jsonField,
      const std::function<Json::Value (sqlite3*)>& cb);

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

  Json::Value GameStateToJson (const GameStateData& state) override;

};

/**
 * Helper class to manage a series of automatically generated IDs that can
 * be used e.g. as primary keys for database tables.  (But other than
 * letting SQLite generate them, they are guaranteed to be consistent
 * across instances and reorgs.)
 *
 * This instance provides the user-visible interface.  It caches the currently
 * next ID in memory, only reading it from the database and syncing it back
 * when constructed/destructed.  One such instance is kept active while the
 * user-supplied game logic is executing, e.g. while processing one block.
 * This ensures that we do not have to do many SQL operations for managing IDs;
 * at most two per block (and per instance).
 */
class SQLiteGame::AutoId
{

private:

  /** The next ID value to give out.  */
  unsigned nextValue;

  /**
   * The last value that has been read from or synced to the database.
   * (Or 0 if no sync has been made yet.)
   */
  unsigned dbValue = 0;

  /**
   * Constructs the AutoId, initialised from the database.
   */
  explicit AutoId (SQLiteGame& game, const std::string& key);

  /**
   * Syncs the current value back to the database if it has been modified.
   */
  void Sync (SQLiteGame& game, const std::string& key);

  friend class SQLiteGame::ActiveAutoIds;

public:

  /**
   * Destructs the AutoId instance.  This crashes if the current value is
   * different from the one synced to the database.  Sync() has to be called
   * before destructing the object to ensure this.
   */
  ~AutoId ();

  AutoId () = delete;
  AutoId (const AutoId&) = delete;
  void operator= (const AutoId&) = delete;

  /**
   * Retrieves the next value.
   */
  unsigned
  GetNext ()
  {
    return nextValue++;
  }

  /**
   * Pre-reserves all IDs up to the given value.  That can be used to mark
   * them unavailable when they have been created or used otherwise, for
   * instance through initial static data.
   */
  void
  ReserveUpTo (const unsigned end)
  {
    nextValue = std::max (nextValue, end + 1);
  }

};

} // namespace xaya

#endif // XAYAGAME_SQLITEGAME_HPP
