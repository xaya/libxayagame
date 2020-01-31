// Copyright (C) 2018-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_SQLITESTORAGE_HPP
#define XAYAGAME_SQLITESTORAGE_HPP

#include "storage.hpp"

#include <xayautil/uint256.hpp>

#include <sqlite3.h>

#include <map>
#include <memory>
#include <string>

namespace xaya
{

/**
 * Wrapper around an SQLite database connection.  This object mostly holds
 * an sqlite3* handle (that is owned and managed by it), but it also
 * provides some extra services like statement caching.
 */
class SQLiteDatabase
{

private:

  /**
   * The SQLite database handle, which is owned and managed by the
   * current instance.  It will be opened in the constructor, and
   * finalised in the destructor.
   */
  sqlite3* db;

  /**
   * A cache of prepared statements (mapping from the SQL command to the
   * statement pointer).
   */
  mutable std::map<std::string, sqlite3_stmt*> preparedStatements;

public:

  /**
   * Opens the database at the given filename into this instance.  The flags
   * are passed on to sqlite3_open_v2.
   */
  explicit SQLiteDatabase (const std::string& file, int flags);

  /**
   * Closes the database connection.
   */
  ~SQLiteDatabase ();

  /**
   * Returns the raw handle of the SQLite database.
   */
  sqlite3*
  operator* ()
  {
    return db;
  }

  /**
   * Returns the raw handle, like operator*.  This function is meant
   * for code that then only does read operations and no writes.
   */
  sqlite3*
  ro () const
  {
    return db;
  }

  /**
   * Prepares an SQL statement given as string and stores it in the cache,
   * or retrieves the existing statement from the cache.  The prepared statement
   * is also reset, so that it can be reused right away.
   *
   * The returned statement is managed (and, in particular, finalised) by the
   * SQLiteDatabase object, not by the caller.
   */
  sqlite3_stmt* Prepare (const std::string& sql);

  /**
   * Prepares an SQL statement given as string like Prepare.  This method
   * is meant for statements that are read-only, i.e. SELECT.
   */
  sqlite3_stmt* PrepareRo (const std::string& sql) const;

};

/**
 * Implementation of StorageInterface, where all data is stored in an SQLite
 * database.  In general, a no-SQL database would be more suitable for game
 * storage (as only key lookups are required), but this can be useful in
 * combination with games that keep their game state in SQLite as well (so that
 * a single database holds everything).
 *
 * The storage implementation here uses tables with prefix "xayagame_".
 * Subclasses that wish to store custom other data must not use tables
 * with this prefix.
 */
class SQLiteStorage : public StorageInterface
{

private:

  /**
   * The filename of the database.  This is needed for resetting the storage,
   * which removes the file and reopens the database.
   */
  const std::string filename;

  /**
   * The database connection we use (mainly) and for writes, if one is
   * opened at the moment.
   */
  std::unique_ptr<SQLiteDatabase> db;

  /**
   * Set to true when we have a currently open transaction.  This is used to
   * verify that BeginTransaction is not called in a nested way.  (Savepoints
   * would in theory support that, but we exclude it nevertheless.)
   */
  bool startedTransaction = false;

  /**
   * Opens the database at filename into db.  It is an error if the
   * database is already opened.
   */
  void OpenDatabase ();

protected:

  /**
   * Sets up the database schema if it does not already exist.  This function is
   * called after opening the database, including when it was first created (but
   * not only then).  It creates the required tables if they do not yet exist.
   *
   * Subclasses can override the method to also set up their schema (and,
   * perhaps, to upgrade the schema when the software version changes).  They
   * should make sure to call the superclass method as well in their
   * implementation.
   */
  virtual void SetupSchema ();

  /**
   * Returns the handle of the SQLite database.
   */
  sqlite3* GetDatabase ();

  /**
   * Prepares an SQL statement given as string and stores it in the cache,
   * or retrieves the existing statement from the cache.  The prepared statement
   * is also reset, so that it can be reused right away.
   *
   * The returned statement is managed (and, in particular, finalised) by the
   * SQLiteStorage object, not by the caller.
   */
  sqlite3_stmt* PrepareStatement (const std::string& sql) const;

  /**
   * Steps a given statement and expects no results (i.e. for an update).
   * Can also be used for statements where we expect exactly one result to
   * verify that no more are there.
   */
  static void StepWithNoResult (sqlite3_stmt* stmt);

public:

  explicit SQLiteStorage (const std::string& f);

  SQLiteStorage () = delete;
  SQLiteStorage (const SQLiteStorage&) = delete;
  void operator= (const SQLiteStorage&) = delete;

  void Initialise () override;

  /**
   * Clears the storage.  This deletes and re-creates the full database,
   * and does not only delete from the tables that SQLiteStorage itself uses.
   * This ensures that all data, including about game states, that is stored
   * in the same database is removed consistently.
   */
  void Clear () override;

  bool GetCurrentBlockHash (uint256& hash) const override;
  GameStateData GetCurrentGameState () const override;
  void SetCurrentGameState (const uint256& hash,
                            const GameStateData& data) override;

  bool GetUndoData (const uint256& hash, UndoData& data) const override;
  void AddUndoData (const uint256& hash,
                    unsigned height, const UndoData& data) override;
  void ReleaseUndoData (const uint256& hash) override;
  void PruneUndoData (unsigned height) override;

  void BeginTransaction () override;
  void CommitTransaction () override;
  void RollbackTransaction () override;

};

} // namespace xaya

#endif // XAYAGAME_SQLITESTORAGE_HPP
