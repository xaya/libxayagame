// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_HEIGHTCACHE_HPP
#define XAYAGAME_HEIGHTCACHE_HPP

/* This file is an implementation detail of Game and should not be
   used directly by external code!  */

#include "storage.hpp"

#include <xayautil/uint256.hpp>

#include <functional>

namespace xaya
{
namespace internal
{

/**
 * Wrapper around a StorageInterface that adds an in-memory cached height
 * for the current game state.  The SetCurrentGameState function is replaced
 * by a variant that gets the height as well, and a new
 * GetCurrentGameStateWithHeight method is provided to access the associated
 * height as well.
 *
 * When no cached height is available yet or for cross-checking in regtest
 * mode, a function has to be provided that retrieves the block height for
 * a block hash (e.g. by calling Xaya Core's RPC interface).  That is used
 * for cases when the height is requested right after start-up and before
 * it has been set.
 */
class StorageWithCachedHeight : public StorageInterface
{

private:

  /**
   * Callback function that retrieves the block height for a given hash.
   */
  using HeightCallback = std::function<unsigned (const uint256& hash)>;

  /** The callback function we use to translate hashes to heights.  */
  const HeightCallback hashToHeight;

  /** The wrapped storage interface.  */
  StorageInterface* const storage;

  /**
   * If true, then hashes are always translated to heights via the callback
   * (even if there is a cached height).  The cached height, if any, is then
   * cross-checked against the retrieved one.  This can be used for testing
   * purposes, e.g. in regtest mode.
   */
  bool crossCheck = false;

  /**
   * The cached height corresponding to the current game-state block hash
   * in storage.
   */
  mutable unsigned cachedHeight;

  /** Whether or not we have a cached height.  */
  mutable bool hasHeight = false;

  friend class StorageWithDummyHeight;

public:

  explicit StorageWithCachedHeight (StorageInterface& s,
                                    const HeightCallback& cb);

  StorageWithCachedHeight () = delete;
  StorageWithCachedHeight (const StorageWithCachedHeight&) = delete;
  void operator= (const StorageWithCachedHeight&) = delete;

  /**
   * Turns on strict (but expensive) cross checks of the cached height.
   * Game uses this on the regtest chain only.
   */
  void
  EnableCrossChecks ()
  {
    crossCheck = true;
  }

  /**
   * Sets the current game state in the underlying storage, including an
   * associated block height that is cached in memory.
   */
  void SetCurrentGameStateWithHeight (const uint256& hash,
                                      unsigned height,
                                      const GameStateData& data);

  /**
   * Retrieves the current block hash (if any) together with the associated
   * block height.
   */
  bool GetCurrentBlockHashWithHeight (uint256& hash, unsigned& height) const;

  /* Methods from StorageInterface.  They simply call through to the wrapped
     instance, with a few minor extra things.  */

  void
  Initialise () override
  {
    storage->Initialise ();
  }

  void
  Clear () override
  {
    hasHeight = false;
    storage->Clear ();
  }

  bool
  GetCurrentBlockHash (uint256& hash) const override
  {
    return storage->GetCurrentBlockHash (hash);
  }

  GameStateData
  GetCurrentGameState () const override
  {
    return storage->GetCurrentGameState ();
  }

  /**
   * SetCurrentGameState must not be called.  Instead,
   * SetCurrentGameStateWithHeight has to be used.  This method crashes always.
   */
  void SetCurrentGameState (const uint256& hash,
                            const GameStateData& data) override;

  bool
  GetUndoData (const uint256& hash, UndoData& data) const override
  {
    return storage->GetUndoData (hash, data);
  }

  void
  AddUndoData (const uint256& hash, const unsigned height,
               const UndoData& data) override
  {
    storage->AddUndoData (hash, height, data);
  }

  void
  ReleaseUndoData (const uint256& hash) override
  {
    storage->ReleaseUndoData (hash);
  }

  void
  PruneUndoData (const unsigned height) override
  {
    storage->PruneUndoData (height);
  }

  void
  BeginTransaction () override
  {
    storage->BeginTransaction ();
  }

  void
  CommitTransaction () override
  {
    storage->CommitTransaction ();
  }

  void
  RollbackTransaction () override
  {
    /* Clear the cached height to make sure it is not wrong afterwards.  */
    hasHeight = false;

    storage->RollbackTransaction ();
  }

};

} // namespace internal
} // namespace xaya

#endif // XAYAGAME_HEIGHTCACHE_HPP
