// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_STORAGE_HPP
#define XAYAGAME_STORAGE_HPP

#include "uint256.hpp"

#include <map>
#include <string>

namespace xaya
{

/**
 * The game-specific data that encodes a game state.  std::string is used
 * as a convenient container, but games are advised to actually use binary
 * encoding for more compact storage.  Protocol Buffers may be a good
 * way to encode state data (although of course not mandatory).
 */
using GameStateData = std::string;
/** The game-specific undo data for a block.  */
using UndoData = std::string;

/**
 * Interface for the storage layer used by the game.  This is used to
 * hold undo data for every block in the currently active chain as well
 * as the current game state (and its associated block hash).
 *
 * This class is not thread-safe; if used from multiple threads at the same
 * time (e.g. the ZMQ listener and the main thread during start-up), it has
 * to be properly synchronised (also to get a consistent view).
 */
class StorageInterface
{

public:

  virtual ~StorageInterface () = default;

  /**
   * Removes all data, corresponding to a full reset of the state
   * (e.g. for starting a sync from scratch).
   */
  virtual void Clear () = 0;

  /**
   * Retrieves the block hash to which the current game state belongs.  Returns
   * false if there is no "current" game state.
   */
  virtual bool GetCurrentBlockHash (uint256& hash) const = 0;

  /**
   * Retrieves the current game state.  Must not be called if there is none
   * (i.e. if GetCurrentBlockHash returns false).
   */
  virtual GameStateData GetCurrentGameState () const = 0;

  /**
   * Updates the current game state and associated block hash.
   */
  virtual void SetCurrentGameState (const uint256& hash,
                                    const GameStateData& data) = 0;

  /**
   * Retrieves undo data for the given block hash.  Returns false if none is
   * stored with that key.
   */
  virtual bool GetUndoData (const uint256& hash, UndoData& data) const = 0;

  /**
   * Adds undo data for the given block hash.  If there is already undo data
   * for the given hash, then the passed-in data must be equivalent from the
   * game's point of view.  It is undefined which one is kept.
   *
   * Also the height can be stored by the implementation, to be used with
   * PruneUndoData.  Apart from the ability to implement this function,
   * the height is not needed for anything else.
   */
  virtual void AddUndoData (const uint256& hash,
                            unsigned height, const UndoData& data) = 0;

  /**
   * Allows the storage implementation to delete the undo data associated to
   * the given block hash.
   */
  virtual void
  ReleaseUndoData (const uint256& hash)
  {
    /* Do nothing by default.  This function can be overridden to free space
       for no longer required data (e.g. undo data of blocks that have been
       detached).  */
  }

  /**
   * Allows the storage to release all undo data with heights up to (including)
   * the given height.
   */
  virtual void
  PruneUndoData (const unsigned height)
  {
    /* Do nothing by default.  This function can be overridden to free space
       for very old undo data, which is unlikely to be needed again in the
       future (because the blocks involved have many confirmations).  */
  }

};

/**
 * An implementation of the StorageInterface that holds all data just in
 * memory.  This means that it has to resync on every restart, but may be
 * quick and easy for testing / prototyping.
 *
 * Besides needing to sync from scratch on every restart, this is actually
 * a fully functional implementation.
 */
class MemoryStorage : public StorageInterface
{

private:

  /** Whether or not we have a current block hash / state.  */
  bool hasState = false;

  /** The current block hash, if we have one.  */
  uint256 currentBlock;
  /** The current game state.  */
  GameStateData currentState;

  /**
   * Convenience struct to hold a block height together with undo data.
   */
  struct HeightAndUndoData
  {
    unsigned height;
    UndoData data;
  };

  /** Type of the map holding undo data.  */
  using UndoMap = std::map<uint256, HeightAndUndoData>;

  /** Undo data associated to block hashes we know about.  */
  UndoMap undoData;

public:

  MemoryStorage () = default;
  MemoryStorage (const MemoryStorage&) = delete;

  void operator= (const MemoryStorage&) = delete;

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

};

} // namespace xaya

#endif // XAYAGAME_STORAGE_HPP
