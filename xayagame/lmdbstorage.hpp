// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_LMDBSTORAGE_HPP
#define XAYAGAME_LMDBSTORAGE_HPP

#include "storage.hpp"
#include "uint256.hpp"

#include <lmdb.h>

namespace xaya
{

/**
 * Implementation of StorageInterface that keeps data in an LMDB database.
 * This is an efficient choice for permanent storage if no other features
 * (like an SQL interface) are needed for the game itself.
 */
class LMDBStorage : public StorageInterface
{

private:

  class ReadTransaction;
  class Cursor;

  /**
   * Directory for the database.  This is used to open the environment
   * in the Initialise() function call.
   */
  const std::string directory;

  /** The LMDB environment pointer.  */
  MDB_env* env = nullptr;

  /** The currently open DB transaction of null if none.  */
  MDB_txn* startedTxn = nullptr;

  /**
   * The identifier of the opened database in the LMDB environment.  We always
   * use the "unnamed" database.  This field is properly set any time when
   * a transaction is started (startedTxn is not null).
   */
  MDB_dbi dbi;

  /**
   * Special flag that is set to true if we encountered an MDB_MAP_FULL error
   * and need to resize the LMDB map after aborting the current transaction
   * (in the next call to RollbackTransaction that is expected to happen
   * "soon").
   */
  mutable bool needsResize = false;

  /**
   * Checks that the error code is zero.  If it is not, LOG(FATAL)'s with the
   * LMDB translation of the error code to a string.  This also takes care of
   * handling MDB_MAP_FULL as a special case, requesting a resize in that case.
   */
  void CheckOk (int code) const;

  /**
   * Increases the database map size.  This must only be called if no current
   * transaction is active (i.e. startedTxn == nullptr).
   */
  void Resize ();

public:

  /**
   * Creates a storage instance that keeps its data in the given directory.
   * The directory must already exist.
   */
  explicit LMDBStorage (const std::string& dir);

  LMDBStorage () = delete;
  LMDBStorage (const LMDBStorage&) = delete;
  void operator= (const LMDBStorage&) = delete;

  ~LMDBStorage ();

  void Initialise () override;

  void Clear () override;

  bool GetCurrentBlockHash (uint256& hash) const override;
  GameStateData GetCurrentGameState () const override;
  void SetCurrentGameState (const uint256& hash,
                            const GameStateData& state) override;

  bool GetUndoData (const uint256& hash, UndoData& undo) const override;
  void AddUndoData (const uint256& hash,
                    unsigned height, const UndoData& undo) override;
  void ReleaseUndoData (const uint256& hash) override;
  void PruneUndoData (unsigned height) override;

  void BeginTransaction () override;
  void CommitTransaction () override;
  void RollbackTransaction () override;

};

} // namespace xaya

#endif // XAYAGAME_LMDBSTORAGE_HPP
