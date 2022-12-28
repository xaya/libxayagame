// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_SQLITEPROC_HPP
#define XAYAGAME_SQLITEPROC_HPP

#include "sqlitestorage.hpp"

#include <json/json.h>

#include <set>
#include <string>

namespace xaya
{

/**
 * A processor is a component that computes some stuff and updates to the
 * game SQLite database after the main game-state updates have been done.
 * This could just be hashing the game state for debugging purposes, or it
 * could be computing some caches useful to the frontend.
 *
 * The processing is done in two steps:  First, a read-only snapshot at a
 * well-defined block / game state is given, on which the computations can be
 * performed, and they can take relatively long on a separate processing
 * thread (without holding up the main GSP's block processing).  Second, once
 * the computation is done, the main database is acquired and the result can
 * be written back.  Ideally the processor should use its own database
 * table (that can be set up specifically with SetupSchema) for that.
 *
 * Results of the computation can be accumulated and stored in the processor
 * instance.  The two methods (Compute and Store) will always be called
 * alternatingly, i.e. when Compute is called, it can store the results
 * into member variables, and no second call to Compute will be done until
 * Store has been called (that can access those member variables).
 *
 * Note that processors should be treated as "optional" and "best effort".
 * Their results must not influence the actual consensus game state.
 */
class SQLiteProcessor
{

private:

  /**
   * If the default rule of "every X blocks" is used to determine when
   * processing is done, this is set to the block interval (X).  If zero,
   * then it has not yet been enabled / set up.
   */
  uint64_t blockInterval = 0;
  /**
   * If blockInterval is used, then this is the modulo at which it runs
   * (i.e. at all blocks N where (N % X) == M).
   */
  uint64_t blockModulo;

protected:

  /**
   * Checks whether or not the processor should run at the given block.
   * By default, it uses a fixed block interval and modulo to determine
   * this, but subclasses may overwrite the check.
   */
  virtual bool ShouldRun (const Json::Value& blockData) const;

  /**
   * Runs the internal computation on a read-only database snapshot for
   * the given block data.  Results should be stored somewhere in the instance,
   * and can be written back to the database later in Store.
   */
  virtual void Compute (const Json::Value& blockData,
                        const SQLiteDatabase& db) = 0;

  /**
   * After Compute() finishes, this method is called with a writable database
   * so that the internally-stored result can be saved.  The call is wrapped
   * with an SQLite SAVEPOINT for atomicity.
   */
  virtual void Store (SQLiteDatabase& db) = 0;

public:

  SQLiteProcessor () = default;
  virtual ~SQLiteProcessor () = default;

  /**
   * This is called when setting up the processor and database, and gives
   * it the chance to set up any specific database tables or schema it needs
   * for writing the results.
   */
  virtual void SetupSchema (SQLiteDatabase& db);

  /**
   * Waits for all potentially still running operations to finish.  This is
   * invoked before the attached database is closed.  Note that the object
   * stays valid, so a new call to Process can be made afterwards as desired
   * (if the database is opened again), and then Finish called again.
   */
  void Finish ();

  /**
   * Checks if the processor should be executed for the given block,
   * and if so, triggers it by calling the subclass-specific Compute and
   * Store methods accordingly.
   */
  void Process (const Json::Value& blockData, SQLiteDatabase& db);

  /**
   * Enables the processor to run every X blocks (with modulo value M).
   */
  void SetInterval (uint64_t intv, uint64_t modulo = 0);

};

/**
 * A processor that hases the database (excluding internal tables)
 * with SHA256 and records (block hash, game-state hash) into a new table.
 */
class SQLiteHasher : public SQLiteProcessor
{

private:

  /** The block hash being processed currently.  */
  uint256 block;
  /** The computed game-state hash of the currently processed block.  */
  uint256 hash;

protected:

  void Compute (const Json::Value& blockData,
                const SQLiteDatabase& db) override;
  void Store (SQLiteDatabase& db) override;

  /**
   * Computes the list of tables to hash.  By default, it is what
   * GetSqliteTables returns as non-internal tables.  Subclasses may override
   * it to use a different list.
   */
  virtual std::set<std::string> GetTables (const SQLiteDatabase& db);

public:

  void SetupSchema (SQLiteDatabase& db) override;

  /**
   * Retrieves the game-state hash stored in the database for the given
   * block hash, if any.  Returns true if a hash was found, and false
   * if none is stored.
   */
  bool GetHash (const SQLiteDatabase& db, const uint256& block,
                uint256& hash) const;

};

} // namespace xaya

#endif // XAYAGAME_SQLITEPROC_HPP
