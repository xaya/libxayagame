// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_DATABASE_HPP
#define GAMECHANNEL_DATABASE_HPP

#include "boardrules.hpp"

#include "proto/metadata.pb.h"

#include <xayautil/uint256.hpp>

#include <sqlite3.h>

#include <memory>

namespace xaya
{

class ChannelGame;

/**
 * Wrapper class around the state of one channel in the database.  This
 * abstracts the database queries away from the other code.
 *
 * Instances of this class should be obtained through the ChannelsTable.
 */
class ChannelData
{

private:

  /** The underlying ChannelGame, through which we access the database.  */
  ChannelGame& game;

  /** The ID of this channel.  */
  uint256 id;

  /** The channel's metadata.  */
  proto::ChannelMetadata metadata;

  /** The channel's current state.  */
  BoardState state;

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
  explicit ChannelData (ChannelGame& g, const uint256& i);

  /**
   * Constructs an instance based on the given result row.
   */
  explicit ChannelData (ChannelGame& g, sqlite3_stmt* row);

  friend class ChannelsTable;

public:

  /**
   * If this instance has been modified, the destructor updates the
   * database to reflect the new state.
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

  const proto::ChannelMetadata&
  GetMetadata () const
  {
    return metadata;
  }

  proto::ChannelMetadata&
  MutableMetadata ()
  {
    dirty = true;
    return metadata;
  }

  const BoardState&
  GetState () const
  {
    return state;
  }

  void
  SetState (const BoardState& s)
  {
    dirty = true;
    state = s;
  }

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

  /** The underlying ChannelGame instance, through which we access the db.  */
  ChannelGame& game;

public:

  /** Movable handle to a channel instance.  */
  using Handle = std::unique_ptr<ChannelData>;

  explicit ChannelsTable (ChannelGame& g)
    : game(g)
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
  sqlite3_stmt* QueryAll ();

  /**
   * Queries for all game channels which have a dispute height less than or
   * equal to the given height.
   */
  sqlite3_stmt* QueryForDisputeHeight (unsigned height);

};

} // namespace xaya

#endif // GAMECHANNEL_DATABASE_HPP
