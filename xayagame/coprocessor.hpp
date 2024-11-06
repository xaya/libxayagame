// Copyright (C) 2023-2024 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_COPROCESSOR_HPP
#define XAYAGAME_COPROCESSOR_HPP

#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <glog/logging.h>

#include <map>
#include <memory>
#include <set>
#include <string>

namespace xaya
{

/**
 * A coprocessor defines a set of extra logic for processing blocks forward
 * and backward.  It is called for processing blocks forward and backward
 * by the Game instance, and while a block is being processed, the GameLogic
 * implementation can access the active coprocessors from the Context to make
 * use of them.
 *
 * One possible use-case is computing and storing extra data alongside the
 * game state, such as "archival logs" that are only created for use by a
 * game frontend but do not otherwise influence the core game state.  In this
 * case, a coprocessor can be set up that stores those events into an external
 * storage (such as a database server), and the game logic in its state-update
 * function can access the coprocessor to tell it about events while doing
 * the main game-state calculations.
 */
class Coprocessor
{

public:

  /**
   * A type of "operation" done on a specific block.
   */
  enum class Op
  {
    /**
     * This block is the game genesis, and we are doing state
     * initialisation.
     */
    INITIALISATION,

    /** This block is being processed forward.  */
    FORWARD,

    /** This block is being processed backwards (undone).  */
    BACKWARD,
  };

  class Block;

  Coprocessor () = default;
  virtual ~Coprocessor () = default;

  /**
   * Request the coprocessor to start an "atomic transaction" for a batch
   * of future updates, if it supports this.  All updates in a block are always
   * wrapped into such a transaction, and possibly updates from many blocks
   * are in a single transaction while syncing.
   */
  virtual void
  BeginTransaction ()
  {}

  /**
   * Requests the coprocessor to commit the currently active atomic
   * transaction to storage.  This is called after BeginTransaction
   * when all changes have been done successfully.
   */
  virtual void
  CommitTransaction ()
  {}

  /**
   * Requests the coprocessor to abort the currently active atomic
   * transaction and discard any changes made since BeginTransaction
   * has been called.
   */
  virtual void
  AbortTransaction ()
  {}

  /**
   * Constructs a block-processor instance for this coprocessor and the
   * given block data.
   */
  virtual std::unique_ptr<Block> ForBlock (const Json::Value& blockData,
                                           Op op) = 0;

};

/**
 * A list of coprocessors, each named by a string as key.
 */
class CoprocessorBatch
{

private:

  /** The included coprocessors, not owned by this instance.  */
  std::map<std::string, Coprocessor*> processors;

  /** Whether or not an atomic transaction is currently active.  */
  bool activeTransaction = false;

public:

  class Block;

  CoprocessorBatch () = default;
  CoprocessorBatch (const CoprocessorBatch&) = delete;
  void operator= (const CoprocessorBatch&) = delete;

  /**
   * Adds a new coprocessor to the batch.  This instance does not take
   * over ownership, so the reference must remain valid for as long as the
   * CoprocessorBatch lives.
   */
  void Add (const std::string& name, Coprocessor& p);

  void BeginTransaction ();
  void CommitTransaction ();
  void AbortTransaction ();

};

/**
 * Processing instance for a single block.  Implementations of this interface
 * are constructed by a Coprocessor.  The instance will live during processing
 * of one block, either forward or backward.  It will then be committed or
 * aborted (if an error occurs while running the GameLogic for that block),
 * so it can use this for transactional integrity if desired.
 *
 * The active block processor instance is what the GameLogic can query for
 * from the Context while the state update is executing.
 */
class Coprocessor::Block
{

private:

  /** The full block data as JSON.  */
  const Json::Value& blockData;

  /** The operation being done.  */
  const Op op;

  /** The block's hash.  */
  uint256 hash;

  /** The block's height.  */
  uint64_t height;

  friend class CoprocessorBatch::Block;

protected:

  /**
   * Signals to the implementation that it should start processing, with
   * any specific things it would need for that (such as any initialisation).
   * This is called after the constructor.
   *
   * This is separate from the constructor, as that has some technical
   * advantages (such as more well-defined behaviour in case of exceptions
   * or calls to virtual methods).
   */
  virtual void
  Start ()
  {}

  /**
   * Signals to the implementation that processing the block is finished and
   * was successful, so any final processing can take place.
   *
   * If some kind of error occurs, then it may happen that no Finish() is
   * called after Start().  The implementation must be able to handle this.
   * In this case, AbortTransaction() on the Coprocessor will be called
   * at some point.
   */
  virtual void
  Finish ()
  {}

public:

  Block (const Json::Value& d, Op o);
  virtual ~Block () = default;

  /**
   * Returns the block's JSON data as convenience.
   */
  const Json::Value&
  GetBlockData () const
  {
    return blockData;
  }

  /**
   * Returns the block's hash.
   */
  const uint256&
  GetBlockHash () const
  {
    return hash;
  }

  /**
   * Returns the block's height.
   */
  uint64_t
  GetBlockHeight () const
  {
    return height;
  }

  /**
   * Returns the operation being done on the block.
   */
  Op
  GetOperation () const
  {
    return op;
  }

};

/**
 * An instance representing the processing of a block by all coprocessors
 * in a CoprocessorBatch.
 */
class CoprocessorBatch::Block
{

private:

  /** All the individual block processors.  */
  std::map<std::string, std::unique_ptr<Coprocessor::Block>> blocks;

public:

  /**
   * Constructs the block for the given batch of coprocessors and block data.
   */
  Block (CoprocessorBatch& batch, const Json::Value& blockData,
         Coprocessor::Op op);

  /**
   * Calls Start() on all of the coprocessors.
   */
  void Start ();

  /**
   * Mark the block processing as completed with success.
   */
  void Finish ();

  /**
   * Gets the coprocessor block with the given name or null if none exists.
   * The result is dynamic-casted to the requested type, which must be
   * the correct runtime type.
   */
  template <typename T>
    T*
    Get (const std::string& name)
  {
    const auto mit = blocks.find (name);
    if (mit == blocks.end ())
      return nullptr;

    auto* res = dynamic_cast<T*> (mit->second.get ());
    CHECK (res != nullptr)
        << "Wrong dynamic type of coprocessor '" << name << "'";
    return res;
  }

};

} // namespace xaya

#endif // XAYAGAME_COPROCESSOR_HPP
