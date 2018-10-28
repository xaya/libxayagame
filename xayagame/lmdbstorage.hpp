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
