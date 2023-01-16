// Copyright (C) 2018-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_SQLITESTORAGE_HPP
#define XAYAGAME_SQLITESTORAGE_HPP

#include "storage.hpp"

#include <xayautil/uint256.hpp>

#include <sqlite3.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace xaya
{

class SQLiteStorage;

/**
 * Wrapper around an SQLite database connection.  This object mostly holds
 * an sqlite3* handle (that is owned and managed by it), but it also
 * provides some extra services like statement caching.
 */
class SQLiteDatabase
{

public:

  class Statement;

private:

  struct CachedStatement;

  /** Whether or not we have already initialised SQLite.  */
  static bool sqliteInitialised;

  /**
   * Mutex for access to db itself.  We configure the database to be in
   * multi-thread mode (rather than serialised) since statements are
   * created for single-thread use anyway, and thus have to explicitly
   * synchronise any direct access to db.
   */
  mutable std::mutex mutDb;

  /**
   * The SQLite database handle, which is owned and managed by the
   * current instance.  It will be opened in the constructor, and
   * finalised in the destructor.
   */
  sqlite3* db;

  /**
   * Whether or not we have WAL mode on the database.  This is required
   * to support snapshots.  It may not be the case if we have an in-memory
   * database.
   */
  bool walMode;

  /** The "parent" storage if this is a read-only snapshot.  */
  const SQLiteStorage* parent = nullptr;

  /**
   * Mutex protecting the statement cache (but not the statements
   * themselves inside, which have their own locks).
   */
  mutable std::mutex mutPreparedStatements;

  /**
   * A cache of prepared statements (mapping from the SQL command to the
   * statement plus lock).  One SQL string may point to multiple cached
   * entries, in case some of them are currently in use.
   */
  mutable std::multimap<std::string, std::unique_ptr<CachedStatement>>
      preparedStatements;

  /**
   * Marks this is a read-only snapshot (with the given parent storage).  When
   * called, this starts a read transaction to ensure that the current view is
   * preserved for all future queries.  It also registers this as outstanding
   * snapshot with the parent.
   */
  void SetReadonlySnapshot (const SQLiteStorage& p);

  /**
   * Clears the cache of prepared statements.
   */
  void ClearStatementCache ();

  /**
   * Returns whether or not the database is using WAL mode.
   */
  bool
  IsWalMode () const
  {
    return walMode;
  }

  friend class SQLiteStorage;

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
   * Executes a given callback with access to the raw database handle, ensuring
   * necessary locking.  This should typically only be used for select use
   * cases; most operations should go through Prepare instead.
   */
  template <typename Fcn>
    auto
    AccessDatabase (const Fcn& cb)
  {
    std::lock_guard<std::mutex> lock(mutDb);
    return cb (db);
  }

  /**
   * Executes a callback with the raw handle, similar to AccessDatabase.
   * This function is meant for code that then only does read operations
   * and no writes.
   */
  template <typename Fcn>
    auto
    ReadDatabase (const Fcn& cb) const
  {
    std::lock_guard<std::mutex> lock(mutDb);
    return cb (db);
  }

  /**
   * Directly runs a particular SQL statement on the database, without
   * going through a prepared statement.  This can be useful for things like
   * setting up the schema.
   */
  void Execute (const std::string& sql);

  /**
   * Prepares an SQL statement given as string and stores it in the cache,
   * or retrieves the existing statement from the cache.  The prepared statement
   * is also reset, so that it can be reused right away.  The cache takes
   * care of transparently giving out and releasing statements.
   *
   * Note that the returned statement is not thread-safe by itself; but it is
   * fine for multiple threads to concurrently call this method to obtain
   * instances that they can then use.
   */
  Statement Prepare (const std::string& sql);

  /**
   * Prepares an SQL statement given as string like Prepare.  This method
   * is meant for statements that are read-only, i.e. SELECT.
   */
  Statement PrepareRo (const std::string& sql) const;

};

/**
 * An entry into the cache of prepared statements.  It handles cleanup
 * of the sqlite3_stmt, and also holds a flag so that statements can be
 * "acquired" and "released" by threads that work concurrently.
 */
struct SQLiteDatabase::CachedStatement
{

  /** The underlying SQLite statement.  */
  sqlite3_stmt* const stmt;

  /** Whether or not this statement is currently in use.  */
  std::atomic_flag used;

  /**
   * Constructs an instance taking over an existing SQLite statement.
   */
  explicit CachedStatement (sqlite3_stmt* s)
    : stmt(s), used(false)
  {}

  CachedStatement () = delete;
  CachedStatement (const CachedStatement&) = delete;
  void operator= (const CachedStatement&) = delete;

  /**
   * Cleans up the SQLite statement and ensures the statement is not in use.
   */
  ~CachedStatement ();

};

/**
 * Abstraction around an SQLite prepared statement.  It provides some
 * basic utility methods that make working with it easier, and also enables
 * RAII semantics for acquiring / releasing prepared statements from the
 * built-in statement cache.
 */
class SQLiteDatabase::Statement
{

private:

  /** The database this is associated to.  */
  const SQLiteDatabase* db = nullptr;

  /**
   * The underlying cached statement.  The lock is released when this
   * instance goes out of scope.
   */
  CachedStatement* entry = nullptr;

  /**
   * Constructs a statement instance based on the cache entry.  The entry's
   * used flag must already be set by the caller, but will be cleared after this
   * instance goes out of scope.
   */
  explicit Statement (const SQLiteDatabase& d, CachedStatement& s)
    : db(&d), entry(&s)
  {}

  /**
   * Releases the statement referred to and sets it to null.
   */
  void Clear ();

  friend class SQLiteDatabase;

public:

  Statement () = default;
  Statement (Statement&&);
  Statement& operator= (Statement&&);

  ~Statement ();

  Statement (const Statement&) = delete;
  void operator= (const Statement&) = delete;

  /**
   * Exposes the underlying SQLite handle.
   */
  sqlite3_stmt* operator* ();

  /**
   * Returns the underlying sqlite3_stmt handle for const operations
   * (like extracting column values).
   */
  sqlite3_stmt* ro () const;

  /**
   * Executes the statement without expecting any results (i.e. for anything
   * that is not SELECT).
   */
  void Execute ();

  /**
   * Steps the statement.  This asserts that no error is returned.  It returns
   * true if there are more rows (i.e. sqlite3_step returns SQLITE_ROW) and
   * false if not (SQLITE_DONE).
   */
  bool Step ();

  /**
   * Resets the statement without clearing the parameter bindings.
   */
  void Reset ();

  /**
   * Binds a numbered parameter to NULL.
   */
  void BindNull (int ind);

  /**
   * Binds a typed value to a numbered parameter.  Concrete implementations
   * exist for (u)int64_t, (unsigned) int, bool, uint256 and std::string
   * (binding as text).
   */
  template <typename T>
    void Bind (int ind, const T& val);

  /**
   * Binds a numbered parameter to a byte string as BLOB.
   */
  void BindBlob (int ind, const std::string& val);

  /**
   * Checks if the numbered column is NULL in the current row.
   */
  bool IsNull (int ind) const;

  /**
   * Extracts a typed value from the column with the given index in the
   * current row.  Works with int64_t, int, bool, uint256 and
   * std::string (as text).
   */
  template <typename T>
    T Get (int ind) const;

  /**
   * Extracts a byte string as BLOB from a column of the current row.
   */
  std::string GetBlob (int ind) const;

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
   * Number of outstanding snapshots.  This has to drop to zero before
   * we can close the database.
   */
  mutable unsigned snapshots = 0;

  /** Mutex for the snapshot number.  */
  mutable std::mutex mutSnapshots;
  /** Condition variable for waiting for snapshot unrefs.  */
  mutable std::condition_variable cvSnapshots;

  /** Clock used for timing the WAL checkpointing.  */
  using Clock = std::chrono::steady_clock;
  /** Last time when we did a WAL checkpoint.  */
  Clock::time_point lastWalCheckpoint = Clock::time_point::min ();

  /**
   * Opens the database at filename into db.  It is an error if the
   * database is already opened.
   */
  void OpenDatabase ();

  /**
   * Blocks until no read snapshots are open.
   */
  void WaitForSnapshots ();

  /**
   * Decrements the count of outstanding snapshots.
   */
  void UnrefSnapshot () const;

  /**
   * Performs an explicit WAL checkpoint.
   */
  void WalCheckpoint ();

  friend class SQLiteDatabase;

protected:

  /**
   * Closes the database, making sure to wait for all outstanding snapshots.
   * The method is overridden (extended) in the SQLiteGame::Storage subclass.
   */
  virtual void CloseDatabase ();

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
   * Returns the underlying SQLiteDatabase instance.
   */
  SQLiteDatabase& GetDatabase ();

  /**
   * Returns the underlying SQLiteDatabase instance.
   */
  const SQLiteDatabase& GetDatabase () const;

  /**
   * Creates a read-only snapshot of the underlying database and returns
   * the corresponding SQLiteDatabase instance.  May return NULL if the
   * underlying database is not using WAL mode (e.g. in-memory).
   */
  std::unique_ptr<SQLiteDatabase> GetSnapshot () const;

  /**
   * Returns the current block hash (if any) for the given database connection.
   * This method needs to be separated from the instance GetCurrentBlockHash
   * without database argument so that it can be used with snapshots in
   * SQLiteGame.
   */
  static bool GetCurrentBlockHash (const SQLiteDatabase& db, uint256& hash);

public:

  explicit SQLiteStorage (const std::string& f)
    : filename(f)
  {}

  ~SQLiteStorage ();

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
