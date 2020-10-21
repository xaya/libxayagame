// Copyright (C) 2018-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqlitestorage.hpp"

#include <glog/logging.h>

#include <cstdio>
#include <limits>

namespace xaya
{

/* ************************************************************************** */

sqlite3_stmt*
SQLiteDatabase::Statement::operator* ()
{
  CHECK (stmt != nullptr) << "Statement is empty";
  return stmt;
}

sqlite3_stmt*
SQLiteDatabase::Statement::ro () const
{
  CHECK (stmt != nullptr) << "Statement is empty";
  return stmt;
}

void
SQLiteDatabase::Statement::Execute ()
{
  CHECK (!Step ());
}

bool
SQLiteDatabase::Statement::Step ()
{
  const int rc = sqlite3_step (**this);
  switch (rc)
    {
    case SQLITE_ROW:
      return true;
    case SQLITE_DONE:
      return false;
    default:
      LOG (FATAL) << "Unexpected SQLite step result: " << rc;
    }
}

void
SQLiteDatabase::Statement::Reset ()
{
  /* sqlite3_reset returns an error code if the last execution of the
     statement had an error.  We don't care about that here.  */
  sqlite3_reset (stmt);
}

void
SQLiteDatabase::Statement::BindNull (const int ind)
{
  CHECK_EQ (sqlite3_bind_null (**this, ind), SQLITE_OK);
}

template <>
  void
  SQLiteDatabase::Statement::Bind<int64_t> (const int ind, const int64_t& val)
{
  CHECK_EQ (sqlite3_bind_int64 (**this, ind, val), SQLITE_OK);
}

template <>
  void
  SQLiteDatabase::Statement::Bind<uint64_t> (const int ind, const uint64_t& val)
{
  CHECK_LE (val, std::numeric_limits<int64_t>::max ());
  Bind<int64_t> (ind, val);
}

template <>
  void
  SQLiteDatabase::Statement::Bind<int> (const int ind, const int& val)
{
  Bind<int64_t> (ind, val);
}

template <>
  void
  SQLiteDatabase::Statement::Bind<unsigned> (const int ind, const unsigned& val)
{
  Bind<uint64_t> (ind, val);
}

template <>
  void
  SQLiteDatabase::Statement::Bind<bool> (const int ind, const bool& val)
{
  Bind<int> (ind, val);
}

template <>
  void
  SQLiteDatabase::Statement::Bind<uint256> (const int ind, const uint256& val)
{
  CHECK_EQ (sqlite3_bind_blob (**this, ind, val.GetBlob (), uint256::NUM_BYTES,
                               SQLITE_TRANSIENT),
            SQLITE_OK);
}

template <>
  void
  SQLiteDatabase::Statement::Bind<std::string> (const int ind,
                                                const std::string& val)
{
  CHECK_EQ (sqlite3_bind_text (**this, ind, val.data (), val.size (),
                               SQLITE_TRANSIENT),
            SQLITE_OK);
}

void
SQLiteDatabase::Statement::BindBlob (const int ind, const std::string& val)
{
  CHECK_EQ (sqlite3_bind_blob (**this, ind, val.data (), val.size (),
                               SQLITE_TRANSIENT),
            SQLITE_OK);
}

bool
SQLiteDatabase::Statement::IsNull (const int ind) const
{
  return sqlite3_column_type (ro (), ind) == SQLITE_NULL;
}

template <>
  int64_t
  SQLiteDatabase::Statement::Get<int64_t> (const int ind) const
{
  return sqlite3_column_int64 (ro (), ind);
}

template <>
  uint64_t
  SQLiteDatabase::Statement::Get<uint64_t> (const int ind) const
{
  const int64_t val = Get<int64_t> (ind);
  CHECK_GE (val, 0);
  return val;
}

template <>
  int
  SQLiteDatabase::Statement::Get<int> (const int ind) const
{
  const int64_t val = Get<int64_t> (ind);
  CHECK_LE (val, std::numeric_limits<int>::max ());
  CHECK_GE (val, std::numeric_limits<int>::min ());
  return val;
}

template <>
  unsigned
  SQLiteDatabase::Statement::Get<unsigned> (const int ind) const
{
  const int val = Get<int> (ind);
  CHECK_GE (val, 0);
  return val;
}

template <>
  bool
  SQLiteDatabase::Statement::Get<bool> (const int ind) const
{
  const int val = Get<int> (ind);
  CHECK (val == 0 || val == 1);
  return val != 0;
}

template <>
  uint256
  SQLiteDatabase::Statement::Get<uint256> (const int ind) const
{
  const int len = sqlite3_column_bytes (ro (), ind);
  CHECK_EQ (len, uint256::NUM_BYTES);
  const void* data = sqlite3_column_blob (ro (), ind);

  uint256 res;
  res.FromBlob (static_cast<const unsigned char*> (data));

  return res;
}

template <>
  std::string
  SQLiteDatabase::Statement::Get<std::string> (const int ind) const
{
  const int len = sqlite3_column_bytes (ro (), ind);
  if (len == 0)
    return std::string ();

  const unsigned char* str = sqlite3_column_text (ro (), ind);
  CHECK (str != nullptr);
  return std::string (reinterpret_cast<const char*> (str), len);
}

std::string
SQLiteDatabase::Statement::GetBlob (const int ind) const
{
  const int len = sqlite3_column_bytes (ro (), ind);
  if (len == 0)
    return std::string ();

  const void* data = sqlite3_column_blob (ro (), ind);
  CHECK (data != nullptr);
  return std::string (reinterpret_cast<const char*> (data), len);
}

/* ************************************************************************** */

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

} // anonymous namespace

bool SQLiteDatabase::sqliteInitialised = false;

SQLiteDatabase::SQLiteDatabase (const std::string& file, const int flags)
  : db(nullptr)
{
  if (!sqliteInitialised)
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

      CHECK_EQ (sqlite3_config (SQLITE_CONFIG_SERIALIZED, nullptr), SQLITE_OK)
          << "Failed to enable serialised mode for SQLite";

      sqliteInitialised = true;
    }

  const int rc = sqlite3_open_v2 (file.c_str (), &db, flags, nullptr);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to open SQLite database: " << file;

  CHECK (db != nullptr);
  LOG (INFO) << "Opened SQLite database successfully: " << file;

  auto stmt = Prepare ("PRAGMA `journal_mode` = WAL");
  CHECK (stmt.Step ());
  const auto mode = stmt.Get<std::string> (0);
  CHECK (!stmt.Step ());
  if (mode == "wal")
    {
      LOG (INFO) << "Set database to WAL mode";
      walMode = true;
    }
  else
    {
      LOG (WARNING) << "Failed to set WAL mode, journaling is " << mode;
      walMode = false;
    }
}

SQLiteDatabase::~SQLiteDatabase ()
{
  if (parent != nullptr)
    {
      LOG (INFO) << "Ending snapshot read transaction";
      PrepareRo ("ROLLBACK").Execute ();
    }

  for (const auto& stmt : preparedStatements)
    {
      /* sqlite3_finalize returns the error code corresponding to the last
         evaluation of the statement, not an error code "about" finalising it.
         Thus we want to ignore it here.  */
      sqlite3_finalize (stmt.second);
    }

  CHECK (db != nullptr);
  const int rc = sqlite3_close (db);
  if (rc != SQLITE_OK)
    LOG (ERROR) << "Failed to close SQLite database";

  if (parent != nullptr)
    parent->UnrefSnapshot ();
}

void
SQLiteDatabase::SetReadonlySnapshot (const SQLiteStorage& p)
{
  CHECK (parent == nullptr);
  parent = &p;
  LOG (INFO) << "Starting read transaction for snapshot";

  /* There is no way to do an "immediate" read transaction.  Thus we have
     to start a default deferred one, and then issue some SELECT query
     that we don't really care about and that is guaranteed to work.  */

  PrepareRo ("BEGIN").Execute ();

  auto stmt = PrepareRo ("SELECT COUNT(*) FROM `sqlite_master`");
  CHECK (stmt.Step ());
  CHECK (!stmt.Step ());
}

SQLiteDatabase::Statement
SQLiteDatabase::Prepare (const std::string& sql)
{
  return PrepareRo (sql);
}

SQLiteDatabase::Statement
SQLiteDatabase::PrepareRo (const std::string& sql) const
{
  CHECK (db != nullptr);
  const auto mit = preparedStatements.find (sql);
  if (mit != preparedStatements.end ())
    {
      CHECK_EQ (sqlite3_clear_bindings (mit->second), SQLITE_OK);

      auto res = Statement (mit->second);
      res.Reset ();

      return res;
    }

  sqlite3_stmt* res = nullptr;
  const int rc = sqlite3_prepare_v2 (db, sql.c_str (), sql.size () + 1,
                                     &res, nullptr);
  if (rc != SQLITE_OK)
    LOG (FATAL) << "Failed to prepare SQL statement: " << rc;

  preparedStatements.emplace (sql, res);
  return Statement (res);
}

/* ************************************************************************** */

SQLiteStorage::~SQLiteStorage ()
{
  if (db != nullptr)
    CloseDatabase ();
}

void
SQLiteStorage::OpenDatabase ()
{
  CHECK (db == nullptr);
  db = std::make_unique<SQLiteDatabase> (filename,
          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

  SetupSchema ();
}

void
SQLiteStorage::CloseDatabase ()
{
  CHECK (db != nullptr);

  std::unique_lock<std::mutex> lock(mutSnapshots);
  LOG_IF (INFO, snapshots > 0)
      << "Waiting for outstanding snapshots to be finished...";
  while (snapshots > 0)
    cvSnapshots.wait (lock);

  db.reset ();
}

SQLiteDatabase&
SQLiteStorage::GetDatabase ()
{
  CHECK (db != nullptr);
  return *db;
}

const SQLiteDatabase&
SQLiteStorage::GetDatabase () const
{
  CHECK (db != nullptr);
  return *db;
}

std::unique_ptr<SQLiteDatabase>
SQLiteStorage::GetSnapshot () const
{
  CHECK (db != nullptr);
  if (!db->IsWalMode ())
    {
      LOG (WARNING) << "Snapshot is not possible for non-WAL database";
      return nullptr;
    }

  std::lock_guard<std::mutex> lock(mutSnapshots);
  ++snapshots;

  auto res = std::make_unique<SQLiteDatabase> (filename, SQLITE_OPEN_READONLY);
  res->SetReadonlySnapshot (*this);

  return res;
}

void
SQLiteStorage::UnrefSnapshot () const
{
  std::lock_guard<std::mutex> lock(mutSnapshots);
  CHECK_GT (snapshots, 0);
  --snapshots;
  cvSnapshots.notify_all ();
}

void
SQLiteStorage::SetupSchema ()
{
  LOG (INFO) << "Setting up database schema if it does not exist yet";
  const int rc = sqlite3_exec (**db, R"(
    CREATE TABLE IF NOT EXISTS `xayagame_current`
        (`key` TEXT PRIMARY KEY,
         `value` BLOB NOT NULL);
    CREATE TABLE IF NOT EXISTS `xayagame_undo`
        (`hash` BLOB PRIMARY KEY,
         `data` BLOB NOT NULL,
         `height` INTEGER NOT NULL);
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
SQLiteStorage::GetCurrentBlockHash (const SQLiteDatabase& db, uint256& hash)
{
  auto stmt = db.PrepareRo (R"(
    SELECT `value`
      FROM `xayagame_current`
      WHERE `key` = 'blockhash'
  )");

  if (!stmt.Step ())
    return false;

  hash = stmt.Get<uint256> (0);
  CHECK (!stmt.Step ());

  return true;
}

bool
SQLiteStorage::GetCurrentBlockHash (uint256& hash) const
{
  return GetCurrentBlockHash (*db, hash);
}

GameStateData
SQLiteStorage::GetCurrentGameState () const
{
  auto stmt = db->Prepare (R"(
    SELECT `value`
      FROM `xayagame_current`
      WHERE `key` = 'gamestate'
  )");

  CHECK (stmt.Step ()) << "Failed to fetch current game state";

  const GameStateData res = stmt.GetBlob (0);
  CHECK (!stmt.Step ());

  return res;
}

void
SQLiteStorage::SetCurrentGameState (const uint256& hash,
                                    const GameStateData& data)
{
  CHECK (startedTransaction);

  db->Prepare ("SAVEPOINT `xayagame-setcurrentstate`").Execute ();

  auto stmt = db->Prepare (R"(
    INSERT OR REPLACE INTO `xayagame_current` (`key`, `value`)
      VALUES ('blockhash', ?1)
  )");
  stmt.Bind (1, hash);
  stmt.Execute ();

  stmt = db->Prepare (R"(
    INSERT OR REPLACE INTO `xayagame_current` (`key`, `value`)
      VALUES ('gamestate', ?1)
  )");
  stmt.BindBlob (1, data);
  stmt.Execute ();

  db->Prepare ("RELEASE `xayagame-setcurrentstate`").Execute ();
}

bool
SQLiteStorage::GetUndoData (const uint256& hash, UndoData& data) const
{
  auto stmt = db->Prepare (R"(
    SELECT `data`
      FROM `xayagame_undo`
      WHERE `hash` = ?1
  )");
  stmt.Bind (1, hash);

  if (!stmt.Step ())
    return false;

  data = stmt.GetBlob (0);
  CHECK (!stmt.Step ());

  return true;
}

void
SQLiteStorage::AddUndoData (const uint256& hash,
                            const unsigned height, const UndoData& data)
{
  CHECK (startedTransaction);

  auto stmt = db->Prepare (R"(
    INSERT OR REPLACE INTO `xayagame_undo` (`hash`, `data`, `height`)
      VALUES (?1, ?2, ?3)
  )");

  stmt.Bind (1, hash);
  stmt.BindBlob (2, data);
  stmt.Bind (3, height);

  stmt.Execute ();
}

void
SQLiteStorage::ReleaseUndoData (const uint256& hash)
{
  CHECK (startedTransaction);

  auto stmt = db->Prepare (R"(
    DELETE FROM `xayagame_undo`
      WHERE `hash` = ?1
  )");

  stmt.Bind (1, hash);
  stmt.Execute ();
}

void
SQLiteStorage::PruneUndoData (const unsigned height)
{
  CHECK (startedTransaction);

  auto stmt = db->Prepare (R"(
    DELETE FROM `xayagame_undo`
      WHERE `height` <= ?1
  )");
  stmt.Bind (1, height);

  stmt.Execute ();
}

void
SQLiteStorage::BeginTransaction ()
{
  CHECK (!startedTransaction);
  startedTransaction = true;
  db->Prepare ("SAVEPOINT `xayagame-sqlitegame`").Execute ();
}

void
SQLiteStorage::CommitTransaction ()
{
  db->Prepare ("RELEASE `xayagame-sqlitegame`").Execute ();
  CHECK (startedTransaction);
  startedTransaction = false;
}

void
SQLiteStorage::RollbackTransaction ()
{
  db->Prepare ("ROLLBACK TO `xayagame-sqlitegame`").Execute ();
  CHECK (startedTransaction);
  startedTransaction = false;
}

/* ************************************************************************** */

} // namespace xaya
