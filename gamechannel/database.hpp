// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_DATABASE_HPP
#define GAMECHANNEL_DATABASE_HPP

#include "boardrules.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayagame/sqlitestorage.hpp>
#include <xayautil/uint256.hpp>

#include <sqlite3.h>

#include <memory>

namespace xaya
{

/**
 * Wrapper class around the state of one channel in the database.  This
 * abstracts the database queries away from the other code.
 *
 * Instances of this class should be obtained through the ChannelsTable.
 */
class ChannelData
{

private:

  /** The underlying database.  */
  SQLiteDatabase& db;

  /** The ID of this channel.  */
  uint256 id;

  /** The channel's metadata.  */
  proto::ChannelMetadata metadata;

  /** The channel's reinitialisation state.  */
  BoardState reinit;

  /** The latest state proof.  */
  proto::StateProof proof;

  /**
   * Set to true if we have initialised metadata and reinit state.  This is
   * false initially for newly constructed channels.
   */
  bool initialised;

  /** The dispute height or 0 if there is no dispute.  */
  unsigned disputeHeight;

  /**
   * Set to true if the data has been modified and we need to update the
   * database table in the destructor.
   */
  bool dirty;

  /**
   * Constructs a new instance for the given ID.
   */
  explicit ChannelData (SQLiteDatabase& db, const uint256& i);

  /**
   * Constructs an instance based on the given result row.
   */
  explicit ChannelData (SQLiteDatabase& db, sqlite3_stmt* row);

  friend class ChannelsTable;

public:

  /**
   * If this instance has been modified, the destructor updates the
   * database to reflect the changes not directly saved to the DB.
   */
  ~ChannelData ();

  ChannelData () = delete;
  ChannelData (const ChannelData&) = delete;
  void operator= (const ChannelData&) = delete;

  const uint256&
  GetId () const
  {
    return id;
  }

  const proto::ChannelMetadata& GetMetadata () const;

  const BoardState& GetReinitState () const;

  /**
   * Reinitialises the channel.  This allows changes to the metadata, purges
   * all archived states and sets the state to the given initial state.
   */
  void Reinitialise (const proto::ChannelMetadata& m,
                     const BoardState& initialState);

  const proto::StateProof& GetStateProof () const;
  const BoardState& GetLatestState () const;
  void SetStateProof (const proto::StateProof& p);

  bool
  HasDispute () const
  {
    return disputeHeight > 0;
  }

  unsigned GetDisputeHeight () const;

  void
  ClearDispute ()
  {
    dirty = true;
    disputeHeight = 0;
  }

  void SetDisputeHeight (unsigned h);

};

/**
 * Utility class that handles querying and modifying the channels table in the
 * database.  This class provides ChannelData instances.
 */
class ChannelsTable
{

private:

  /** The underlying database instance.  */
  SQLiteDatabase& db;

public:

  /** Movable handle to a channel instance.  */
  using Handle = std::unique_ptr<ChannelData>;

  explicit ChannelsTable (SQLiteDatabase& d)
    : db(d)
  {}

  ChannelsTable () = delete;
  ChannelsTable (const ChannelsTable&) = delete;
  void operator= (const ChannelsTable&) = delete;

  /**
   * Returns a handle for the instance based on the result row.
   */
  Handle GetFromResult (sqlite3_stmt* row);

  /**
   * Returns a handle by ID of the channel.  Returns null if no such channel
   * is in the database.
   */
  Handle GetById (const uint256& id);

  /**
   * Creates a new handle in the database.
   */
  Handle CreateNew (const uint256& id);

  /**
   * Deletes the channel entry with the given ID.
   */
  void DeleteById (const uint256& id);

  /**
   * Queries for all game channels.  The returned sqlite3_stmt can be walked
   * through and used with GetFromResult, but should not be freed.
   */
  SQLiteDatabase::Statement QueryAll ();

  /**
   * Queries for all game channels which have a dispute height less than or
   * equal to the given height.
   */
  SQLiteDatabase::Statement QueryForDisputeHeight (unsigned height);

};

} // namespace xaya

#endif // GAMECHANNEL_DATABASE_HPP
