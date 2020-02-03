// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "database.hpp"

#include "stateproof.hpp"

#include <glog/logging.h>

#include <set>

namespace xaya
{

namespace
{

/* Indices of the columns for the channels table from SELECT's.  */
constexpr int COLUMN_ID = 0;
constexpr int COLUMN_METADATA = 1;
constexpr int COLUMN_REINIT = 2;
constexpr int COLUMN_STATEPROOF = 3;
constexpr int COLUMN_DISPUTEHEIGHT = 4;

/**
 * Extracts a binary string from a BLOB column.
 */
std::string
ExtractBlobString (sqlite3_stmt* stmt, const int ind)
{
  const int len = sqlite3_column_bytes (stmt, ind);
  const void* data = sqlite3_column_blob (stmt, ind);

  return std::string (static_cast<const char*> (data), len);
}

/**
 * Binds a binary string to a BLOB parameter.
 */
void
BindBlobString (sqlite3_stmt* stmt, const int ind, const std::string& str)
{
  CHECK_EQ (sqlite3_bind_blob (stmt, ind, &str[0], str.size (),
                               SQLITE_TRANSIENT),
            SQLITE_OK);
}

/**
 * Binds an uint256 value to a BLOB parameter.
 */
void
BindBlobUint256 (sqlite3_stmt* stmt, const int ind, const uint256& val)
{
  CHECK_EQ (sqlite3_bind_blob (stmt, ind, val.GetBlob (), uint256::NUM_BYTES,
                               SQLITE_TRANSIENT),
            SQLITE_OK);
}

/**
 * Binds a protocol buffer message to a BLOB parameter.
 */
template <typename Proto>
  void
  BindBlobProto (sqlite3_stmt* stmt, const int ind, const Proto& msg)
{
  std::string serialised;
  CHECK (msg.SerializeToString (&serialised));
  BindBlobString (stmt, ind, serialised);
}

/**
 * Sets a state proof to be just based on the reinitialisation state.
 */
void
StateProofFromReinit (const BoardState& reinit, proto::StateProof& proof)
{
  proof.Clear ();
  proof.mutable_initial_state ()->set_data (reinit);
}

} // anonymous namespace

ChannelData::ChannelData (SQLiteDatabase& d, const uint256& i)
  : db(d), id(i), initialised(false), disputeHeight(0), dirty(true)
{
  LOG (INFO) << "Created new ChannelData instance for ID " << id.ToHex ();
}

ChannelData::ChannelData (SQLiteDatabase& d, sqlite3_stmt* row)
  : db(d), initialised(true), dirty(false)
{
  const int len = sqlite3_column_bytes (row, COLUMN_ID);
  CHECK_EQ (len, uint256::NUM_BYTES);
  const void* data = sqlite3_column_blob (row, COLUMN_ID);
  id.FromBlob (static_cast<const unsigned char*> (data));

  CHECK (metadata.ParseFromString (ExtractBlobString (row, COLUMN_METADATA)));
  reinit = ExtractBlobString (row, COLUMN_REINIT);

  /* See if there is an explicit state proof in the database.  If not, we just
     set it to one based on the reinit state.  */
  if (sqlite3_column_type (row, COLUMN_STATEPROOF) == SQLITE_NULL)
    StateProofFromReinit (reinit, proof);
  else
    CHECK (proof.ParseFromString (ExtractBlobString (row, COLUMN_STATEPROOF)));

  if (sqlite3_column_type (row, COLUMN_DISPUTEHEIGHT) == SQLITE_NULL)
    disputeHeight = 0;
  else
    disputeHeight = sqlite3_column_int64 (row, COLUMN_DISPUTEHEIGHT);

  LOG (INFO)
      << "Created ChannelData instance from result row, ID " << id.ToHex ();
}

ChannelData::~ChannelData ()
{
  CHECK (initialised);

  if (!dirty)
    {
      LOG (INFO) << "ChannelData " << id.ToHex () << " is not dirty";
      return;
    }

  LOG (INFO) << "ChannelData " << id.ToHex () << " is dirty, updating...";

  auto* stmt = db.Prepare (R"(
    INSERT OR REPLACE INTO `xayagame_game_channels`
      (`id`, `metadata`, `reinit`, `stateproof`, `disputeHeight`)
      VALUES (?1, ?2, ?3, ?4, ?5)
  )");

  BindBlobUint256 (stmt, 1, id);
  BindBlobProto (stmt, 2, metadata);
  BindBlobString (stmt, 3, reinit);

  if (GetLatestState () == reinit)
    CHECK_EQ (sqlite3_bind_null (stmt, 4), SQLITE_OK);
  else
    BindBlobProto (stmt, 4, proof);

  if (disputeHeight == 0)
    CHECK_EQ (sqlite3_bind_null (stmt, 5), SQLITE_OK);
  else
    CHECK_EQ (sqlite3_bind_int64 (stmt, 5, disputeHeight), SQLITE_OK);

  CHECK_EQ (sqlite3_step (stmt), SQLITE_DONE);
}

const proto::ChannelMetadata&
ChannelData::GetMetadata () const
{
  CHECK (initialised);
  return metadata;
}

const BoardState&
ChannelData::GetReinitState () const
{
  CHECK (initialised);
  return reinit;
}

void
ChannelData::Reinitialise (const proto::ChannelMetadata& m,
                           const BoardState& initialisedState)
{
  LOG (INFO)
      << "Reinitialising channel " << id.ToHex ()
      << " to new state: " << initialisedState;

  if (initialised)
    CHECK_NE (metadata.reinit (), m.reinit ())
        << "Metadata reinitialisation ID is not changed in reinit of channel";

  metadata = m;
  reinit = initialisedState;
  StateProofFromReinit (reinit, proof);

  initialised = true;
  dirty = true;
}

const proto::StateProof&
ChannelData::GetStateProof () const
{
  CHECK (initialised);
  return proof;
}

const BoardState&
ChannelData::GetLatestState () const
{
  CHECK (initialised);
  return UnverifiedProofEndState (proof);
}

void
ChannelData::SetStateProof (const proto::StateProof& p)
{
  CHECK (initialised);
  dirty = true;
  proof = p;
}

unsigned
ChannelData::GetDisputeHeight () const
{
  CHECK_GT (disputeHeight, 0);
  return disputeHeight;
}

void
ChannelData::SetDisputeHeight (const unsigned h)
{
  CHECK_GT (h, 0);
  dirty = true;
  disputeHeight = h;
}

ChannelsTable::Handle
ChannelsTable::GetFromResult (sqlite3_stmt* row)
{
  return Handle (new ChannelData (db, row));
}

ChannelsTable::Handle
ChannelsTable::GetById (const uint256& id)
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `id`, `metadata`, `reinit`, `stateproof`, `disputeHeight`
      FROM `xayagame_game_channels`
      WHERE `id` = ?1
  )");

  BindBlobUint256 (stmt, 1, id);

  const int rc = sqlite3_step (stmt);
  if (rc == SQLITE_DONE)
    return nullptr;
  CHECK_EQ (rc, SQLITE_ROW);

  auto h = GetFromResult (stmt);
  CHECK_EQ (sqlite3_step (stmt), SQLITE_DONE);

  return h;
}

ChannelsTable::Handle
ChannelsTable::CreateNew (const uint256& id)
{
  return Handle (new ChannelData (db, id));
}

void
ChannelsTable::DeleteById (const uint256& id)
{
  auto* stmt = db.Prepare (R"(
    DELETE FROM `xayagame_game_channels`
      WHERE `id` = ?1
  )");
  BindBlobUint256 (stmt, 1, id);
  CHECK_EQ (sqlite3_step (stmt), SQLITE_DONE);
}

sqlite3_stmt*
ChannelsTable::QueryAll ()
{
  return db.PrepareRo (R"(
    SELECT `id`, `metadata`, `reinit`, `stateproof`, `disputeHeight`
      FROM `xayagame_game_channels`
      ORDER BY `id`
  )");
}

sqlite3_stmt*
ChannelsTable::QueryForDisputeHeight (const unsigned height)
{
  auto* stmt = db.PrepareRo (R"(
    SELECT `id`, `metadata`, `reinit`, `stateproof`, `disputeHeight`
      FROM `xayagame_game_channels`
      WHERE `disputeHeight` <= ?1
      ORDER BY `id`
  )");

  CHECK_EQ (sqlite3_bind_int64 (stmt, 1, height), SQLITE_OK);

  return stmt;
}

} // namespace xaya
