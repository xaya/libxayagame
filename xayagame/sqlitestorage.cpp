// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqlitestorage.hpp"

#include <glog/logging.h>

#include <cstdio>

namespace xaya
{

namespace
{

/**
 * Error callback for SQLite, which prints logs using glog.
 */
void
SQLiteErrorLogger (void* arg, const int errCode, const char* msg)
{
  LOG (ERROR) << "SQLite error (code " << errCode << "): " << msg;
}

/**
 * Binds a BLOB corresponding to an uint256 value to a statement parameter.
 * The value is bound using SQLITE_STATIC, so the uint256's data must not be
 * changed until the statement execution has finished.
 */
void
BindUint256 (sqlite3_stmt* stmt, const int ind, const uint256& value)
{
  const int rc = sqlite3_bind_blob (stmt, ind,
                                    value.GetBlob (), uint256::NUM_BYTES,
                                    SQLITE_STATIC);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to bind uint256 value to parameter: " << rc;
}

/**
 * Binds a BLOB parameter to a std::string value.  The value is bound using
 * SQLITE_STATIC, so the underlying string must remain valid until execution
 * of the prepared statement is done.
 */
void
BindStringBlob (sqlite3_stmt* stmt, const int ind, const std::string& value)
{
  const int rc = sqlite3_bind_blob (stmt, ind, &value[0], value.size (),
                                    SQLITE_STATIC);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to bind string value to parameter: " << rc;
}

/**
 * Retrieves a column value from a BLOB field as std::string.
 */
std::string
GetStringBlob (sqlite3_stmt* stmt, const int ind)
{
  const void* blob = sqlite3_column_blob (stmt, ind);
  const size_t blobSize = sqlite3_column_bytes (stmt, ind);
  return std::string (static_cast<const char*> (blob), blobSize);
}

} // anonymous namespace

SQLiteStorage::SQLiteStorage (const std::string& f)
  : filename(f)
{
  LOG (INFO)
      << "Using SQLite version " << SQLITE_VERSION
      << " (library version: " << sqlite3_libversion () << ")";
  CHECK_EQ (SQLITE_VERSION_NUMBER, sqlite3_libversion_number ())
      << "Mismatch between header and library SQLite versions";

  const int rc
      = sqlite3_config (SQLITE_CONFIG_LOG, &SQLiteErrorLogger, nullptr);
  if (rc != SQLITE_OK)
    LOG (WARNING) << "Failed to set up SQLite error handler: " << rc;
  else
    LOG (INFO) << "Configured SQLite error handler";
}

SQLiteStorage::~SQLiteStorage ()
{
  CloseDatabase ();
}

void
SQLiteStorage::OpenDatabase ()
{
  CHECK (db == nullptr);
  const int rc = sqlite3_open (filename.c_str (), &db);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to open SQLite database: " << filename;

  CHECK (db != nullptr);
  LOG (INFO) << "Opened SQLite database successfully: " << filename;

  SetupSchema ();
}

void
SQLiteStorage::CloseDatabase ()
{
  for (const auto& stmt : preparedStatements)
    {
      /* sqlite3_finalize returns the error code corresponding to the last
         evaluation of the statement, not an error code "about" finalising it.
         Thus we want to ignore it here.  */
      sqlite3_finalize (stmt.second);
    }
  preparedStatements.clear ();

  CHECK (db != nullptr);
  const int rc = sqlite3_close (db);
  if (rc != SQLITE_OK)
    LOG (ERROR) << "Failed to close SQLite database";
  db = nullptr;
}

sqlite3*
SQLiteStorage::GetDatabase ()
{
  CHECK (db != nullptr);
  return db;
}

sqlite3_stmt*
SQLiteStorage::PrepareStatement (const std::string& sql) const
{
  CHECK (db != nullptr);
  const auto mit = preparedStatements.find (sql);
  if (mit != preparedStatements.end ())
    {
      /* sqlite3_reset returns an error code if the last execution of the
         statement had an error.  We don't care about that here.  */
      sqlite3_reset (mit->second);

      const int rc = sqlite3_clear_bindings (mit->second);
      if (rc != SQLITE_OK)
        LOG (ERROR) << "Failed to reset bindings for statement: " << rc;

      return mit->second;
    }

  sqlite3_stmt* res = nullptr;
  const int rc = sqlite3_prepare_v2 (db, sql.c_str (), sql.size () + 1,
                                     &res, nullptr);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to prepare SQL statement: " << rc;

  preparedStatements.emplace (sql, res);
  return res;
}

/**
 * Steps a given statement and expects no results (i.e. for an update).
 * Can also be used for statements where we expect exactly one result to
 * verify that no more are there.
 */
void
SQLiteStorage::StepWithNoResult (sqlite3_stmt* stmt)
{
  const int rc = sqlite3_step (stmt);
  if (rc != SQLITE_DONE)
    LOG (FATAL) << "Expected SQLITE_DONE, got: " << rc;
}

void
SQLiteStorage::SetupSchema ()
{
  LOG (INFO) << "Setting up database schema if it does not exist yet";
  const int rc = sqlite3_exec (db, R"(
    CREATE TABLE IF NOT EXISTS `xayagame_current`
        (`key` TEXT PRIMARY KEY,
         `value` BLOB);
    CREATE TABLE IF NOT EXISTS `xayagame_undo`
        (`hash` BLOB PRIMARY KEY,
         `data` BLOB,
         `height` INTEGER);
  )", nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to set up database schema: " << rc;
}

void
SQLiteStorage::Initialise ()
{
  StorageInterface::Initialise ();
  if (db == nullptr)
    OpenDatabase ();
}

void
SQLiteStorage::Clear ()
{
  CloseDatabase ();

  if (filename == ":memory:")
    LOG (INFO)
        << "Database with filename '" << filename << "' is temporary,"
        << " so it does not need to be explicitly removed";
  else
    {
      LOG (INFO) << "Removing file to clear database: " << filename;
      const int rc = std::remove (filename.c_str ());
      if (rc != 0)
        LOG (FATAL) << "Failed to remove file: " << rc;
    }

  OpenDatabase ();
}

bool
SQLiteStorage::GetCurrentBlockHash (uint256& hash) const
{
  auto* stmt = PrepareStatement (R"(
    SELECT `value` FROM `xayagame_current` WHERE `key` = 'blockhash'
  )");

  const int rc = sqlite3_step (stmt);
  if (rc == SQLITE_DONE)
    return false;
  if (rc != SQLITE_ROW)
    LOG (FATAL) << "Failed to fetch current block hash: " << rc;

  const void* blob = sqlite3_column_blob (stmt, 0);
  const size_t blobSize = sqlite3_column_bytes (stmt, 0);
  CHECK_EQ (blobSize, uint256::NUM_BYTES)
      << "Invalid uint256 value stored in database";
  hash.FromBlob (static_cast<const unsigned char*> (blob));

  StepWithNoResult (stmt);
  return true;
}

GameStateData
SQLiteStorage::GetCurrentGameState () const
{
  auto* stmt = PrepareStatement (R"(
    SELECT `value` FROM `xayagame_current` WHERE `key` = 'gamestate'
  )");

  const int rc = sqlite3_step (stmt);
  if (rc != SQLITE_ROW)
    LOG (FATAL) << "Failed to fetch current game state: " << rc;

  const GameStateData res = GetStringBlob (stmt, 0);

  StepWithNoResult (stmt);
  return res;
}

void
SQLiteStorage::SetCurrentGameState (const uint256& hash,
                                    const GameStateData& data)
{
  CHECK (startedTransaction);

  StepWithNoResult (PrepareStatement ("SAVEPOINT `xayagame-setcurrentstate`"));

  sqlite3_stmt* stmt = PrepareStatement (R"(
    INSERT OR REPLACE INTO `xayagame_current` (`key`, `value`)
      VALUES ('blockhash', ?1)
  )");
  BindUint256 (stmt, 1, hash);
  StepWithNoResult (stmt);

  stmt = PrepareStatement (R"(
    INSERT OR REPLACE INTO `xayagame_current` (`key`, `value`)
      VALUES ('gamestate', ?1)
  )");
  BindStringBlob (stmt, 1, data);
  StepWithNoResult (stmt);

  StepWithNoResult (PrepareStatement ("RELEASE `xayagame-setcurrentstate`"));
}

bool
SQLiteStorage::GetUndoData (const uint256& hash, UndoData& data) const
{
  auto* stmt = PrepareStatement (R"(
    SELECT `data` FROM `xayagame_undo` WHERE `hash` = ?1
  )");
  BindUint256 (stmt, 1, hash);

  const int rc = sqlite3_step (stmt);
  if (rc == SQLITE_DONE)
    return false;
  if (rc != SQLITE_ROW)
    LOG (FATAL) << "Failed to fetch undo data: " << rc;

  data = GetStringBlob (stmt, 0);

  StepWithNoResult (stmt);
  return true;
}

void
SQLiteStorage::AddUndoData (const uint256& hash,
                            const unsigned height, const UndoData& data)
{
  CHECK (startedTransaction);

  auto* stmt = PrepareStatement (R"(
    INSERT OR REPLACE INTO `xayagame_undo` (`hash`, `data`, `height`)
      VALUES (?1, ?2, ?3)
  )");

  BindUint256 (stmt, 1, hash);
  BindStringBlob (stmt, 2, data);

  const int rc = sqlite3_bind_int (stmt, 3, height);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to bind block height value: " << rc;

  StepWithNoResult (stmt);
}

void
SQLiteStorage::ReleaseUndoData (const uint256& hash)
{
  CHECK (startedTransaction);

  auto* stmt = PrepareStatement (R"(
    DELETE FROM `xayagame_undo` WHERE `hash` = ?1
  )");

  BindUint256 (stmt, 1, hash);
  StepWithNoResult (stmt);
}

void
SQLiteStorage::PruneUndoData (const unsigned height)
{
  CHECK (startedTransaction);

  auto* stmt = PrepareStatement (R"(
    DELETE FROM `xayagame_undo` WHERE `height` <= ?1
  )");

  const int rc = sqlite3_bind_int (stmt, 1, height);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to bind block height value: " << rc;

  StepWithNoResult (stmt);
}

void
SQLiteStorage::BeginTransaction ()
{
  CHECK (!startedTransaction);
  startedTransaction = true;
  StepWithNoResult (PrepareStatement ("SAVEPOINT `xayagame-sqlitegame`"));
}

void
SQLiteStorage::CommitTransaction ()
{
  StepWithNoResult (PrepareStatement ("RELEASE `xayagame-sqlitegame`"));
  CHECK (startedTransaction);
  startedTransaction = false;
}

void
SQLiteStorage::RollbackTransaction ()
{
  StepWithNoResult (PrepareStatement ("ROLLBACK TO `xayagame-sqlitegame`"));
  CHECK (startedTransaction);
  startedTransaction = false;
}

} // namespace xaya
