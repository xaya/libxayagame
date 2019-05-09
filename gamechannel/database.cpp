// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "database.hpp"

#include "channelgame.hpp"

#include <glog/logging.h>

#include <set>

namespace xaya
{

namespace
{

/* Indices of the columns for the channels table from SELECT's.  */
constexpr int COLUMN_ID = 0;
constexpr int COLUMN_METADATA = 1;
constexpr int COLUMN_STATE = 2;
constexpr int COLUMN_DISPUTEHEIGHT = 3;

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

} // anonymous namespace

ChannelData::ChannelData (ChannelGame& g, const uint256& i)
  : game(g), id(i), disputeHeight(0), dirty(true)
{
  LOG (INFO) << "Created new ChannelData instance for ID " << id.ToHex ();
}

ChannelData::ChannelData (ChannelGame& g, sqlite3_stmt* row)
  : game(g), dirty(false)
{
  int len = sqlite3_column_bytes (row, COLUMN_ID);
  CHECK_EQ (len, uint256::NUM_BYTES);
  const void* data = sqlite3_column_blob (row, COLUMN_ID);
  id.FromBlob (static_cast<const unsigned char*> (data));

  CHECK (metadata.ParseFromString (ExtractBlobString (row, COLUMN_METADATA)));
  state = ExtractBlobString (row, COLUMN_STATE);

  if (sqlite3_column_type (row, COLUMN_DISPUTEHEIGHT) == SQLITE_NULL)
    disputeHeight = 0;
  else
    disputeHeight = sqlite3_column_int64 (row, COLUMN_DISPUTEHEIGHT);

  LOG (INFO)
      << "Created ChannelData instance from result row, ID " << id.ToHex ();
}

ChannelData::~ChannelData ()
{
  if (!dirty)
    {
      LOG (INFO) << "ChannelData " << id.ToHex () << " is not dirty";
      return;
    }

  LOG (INFO) << "ChannelData " << id.ToHex () << " is dirty, updating...";

  auto* stmt = game.PrepareStatement (R"(
    INSERT OR REPLACE INTO `xayagame_game_channels`
      (`id`, `metadata`, `state`, `disputeHeight`)
      VALUES (?1, ?2, ?3, ?4)
  )");

  BindBlobUint256 (stmt, 1, id);

  std::string metadataStr;
  CHECK (metadata.SerializeToString (&metadataStr));
  BindBlobString (stmt, 2, metadataStr);

  BindBlobString (stmt, 3, state);

  if (disputeHeight == 0)
    CHECK_EQ (sqlite3_bind_null (stmt, 4), SQLITE_OK);
  else
    CHECK_EQ (sqlite3_bind_int64 (stmt, 4, disputeHeight),
              SQLITE_OK);

  CHECK_EQ (sqlite3_step (stmt), SQLITE_DONE);
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
  return Handle (new ChannelData (game, row));
}

ChannelsTable::Handle
ChannelsTable::GetById (const uint256& id)
{
  auto* stmt = game.PrepareStatement (R"(
    SELECT `id`, `metadata`, `state`, `disputeHeight`
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
  return Handle (new ChannelData (game, id));
}

void
ChannelsTable::DeleteById (const uint256& id)
{
  auto* stmt = game.PrepareStatement (R"(
    DELETE FROM `xayagame_game_channels`
      WHERE `id` = ?1
  )");

  BindBlobUint256 (stmt, 1, id);

  CHECK_EQ (sqlite3_step (stmt), SQLITE_DONE);
}

sqlite3_stmt*
ChannelsTable::QueryAll ()
{
  return game.PrepareStatement (R"(
    SELECT `id`, `metadata`, `state`, `disputeHeight`
      FROM `xayagame_game_channels`
      ORDER BY `id`
  )");
}

sqlite3_stmt*
ChannelsTable::QueryForDisputeHeight (const unsigned height)
{
  auto* stmt = game.PrepareStatement (R"(
    SELECT `id`, `metadata`, `state`, `disputeHeight`
      FROM `xayagame_game_channels`
      WHERE `disputeHeight` <= ?1
      ORDER BY `id`
  )");

  CHECK_EQ (sqlite3_bind_int64 (stmt, 1, height), SQLITE_OK);

  return stmt;
}

} // namespace xaya
