// Copyright (C) 2018-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqlitestorage.hpp"

#include "perftimer.hpp"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <chrono>
#include <cstdio>
#include <limits>

/**
 * The interval (in milliseconds) at which the database WAL file will
 * be checkpointed and truncated.  If set to zero, we will not do any
 * explicit checkpointing.  To do a checkpoint, all readers must finish
 * first, and then the database remains blocked for any processing until
 * the checkpoint is finished.
 */
DEFINE_int32 (xaya_sqlite_wal_truncate_ms, 0,
              "if non-zero, interval between explicit WAL checkpoints");

/**
 * A duration threshold above which SQLite queries are assumed to be "slow".
 * If a query is slow, it will be WARNING-logged together with timing, instead
 * of just VLOGs.
 */
DEFINE_int32 (xaya_sqlite_slow_query_ms, 0,
              "if non-zero, warn about queries taking longer than this");

namespace xaya
{

/* ************************************************************************** */

SQLiteDatabase::Statement::Statement (Statement&& o)
{
  *this = std::move (o);
}

SQLiteDatabase::Statement&
SQLiteDatabase::Statement::operator= (Statement&& o)
{
  Clear ();

  db = o.db;
  o.db = nullptr;

  entry = o.entry;
  o.entry = nullptr;

  return *this;
}

SQLiteDatabase::Statement::~Statement ()
{
  Clear ();
}

void
SQLiteDatabase::Statement::Clear ()
{
  if (entry != nullptr)
    {
      VLOG (2) << "Releasing cached SQL statement at " << entry;
      entry->used.clear ();
    }
}

sqlite3_stmt*
SQLiteDatabase::Statement::operator* ()
{
  CHECK (entry != nullptr) << "Statement is empty";
  return entry->stmt;
}

sqlite3_stmt*
SQLiteDatabase::Statement::ro () const
{
  CHECK (entry != nullptr) << "Statement is empty";
  return entry->stmt;
}

void
SQLiteDatabase::Statement::Execute ()
{
  CHECK (!Step ());
}

bool
SQLiteDatabase::Statement::Step ()
{
  CHECK (db != nullptr) << "Statement has no associated database";

  PerformanceTimer timer;
  const int rc = sqlite3_step (**this);
  timer.Stop ();

  const auto slow = std::chrono::milliseconds (FLAGS_xaya_sqlite_slow_query_ms);
  if (FLAGS_xaya_sqlite_slow_query_ms > 0
        && timer.Get<std::chrono::milliseconds> () >= slow)
    LOG (WARNING)
        << "SQLite statement slow query (step " << (steps + 1) << "): " << timer
        << "\n" << GetSql ();
  else if (steps == 0)
    VLOG (1)
        << "SQLite statement initial step: " << timer
        << "\n" << GetSql ();
  else
    VLOG (2)
        << "SQLite statement step " << (steps + 1) << ": " << timer
        << "\n" << GetSql ();

  ++steps;

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
  sqlite3_reset (**this);
  steps = 0;
}

std::string
SQLiteDatabase::Statement::GetSql () const
{
  return sqlite3_sql (ro ());
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
  const auto val = Get<int64_t> (ind);
  CHECK_LE (val, std::numeric_limits<unsigned>::max ());
  CHECK_GE (val, 0);
  return static_cast<unsigned> (val);
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
  const void* data = sqlite3_column_blob (ro (), ind);
  const int len = sqlite3_column_bytes (ro (), ind);
  CHECK_EQ (len, uint256::NUM_BYTES);

  uint256 res;
  res.FromBlob (static_cast<const unsigned char*> (data));

  return res;
}

template <>
  std::string
  SQLiteDatabase::Statement::Get<std::string> (const int ind) const
{
  const unsigned char* str = sqlite3_column_text (ro (), ind);
  const int len = sqlite3_column_bytes (ro (), ind);
  if (len == 0)
    return std::string ();

  CHECK (str != nullptr);
  return std::string (reinterpret_cast<const char*> (str), len);
}

std::string
SQLiteDatabase::Statement::GetBlob (const int ind) const
{
  const void* data = sqlite3_column_blob (ro (), ind);
  const int len = sqlite3_column_bytes (ro (), ind);
  if (len == 0)
    return std::string ();

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
  : filename(file)
{
  if (!sqliteInitialised)
    {
      LOG (INFO)
          << "Using SQLite version " << SQLITE_VERSION
          << " (library version: " << sqlite3_libversion () << ")";
      CHECK_EQ (SQLITE_VERSION_NUMBER, sqlite3_libversion_number ())
          << "Mismatch between header and library SQLite versions";

      CHECK (sqlite3_compileoption_used ("ENABLE_SESSION")
              && sqlite3_compileoption_used ("ENABLE_PREUPDATE_HOOK"))
          << "SQLite must be compiled with SQLITE_ENABLE_SESSION"
              " and SQLITE_ENABLE_PREUPDATE_HOOK";
      CHECK (sqlite3_compileoption_used ("ENABLE_SNAPSHOT"))
          << "SQLite must be compiled with SQLITE_ENABLE_SNAPSHOT";

      const int rc
          = sqlite3_config (SQLITE_CONFIG_LOG, &SQLiteErrorLogger, nullptr);
      if (rc != SQLITE_OK)
        LOG (WARNING) << "Failed to set up SQLite error handler: " << rc;
      else
        LOG (INFO) << "Configured SQLite error handler";

      sqliteInitialised = true;
    }

  /* We open the database connection with full internal locking by SQLite
     to make sure everything is thread safe and we do not need to worry about
     this ourselves in edge cases.  However, between threads in libxayagame,
     we typically use entirely separate connections anyway, such as one
     for writing during block processing, and one for each read as part
     of an RPC request.  */
  const int rc = sqlite3_open_v2 (filename.c_str (), &db,
                                  flags | SQLITE_OPEN_FULLMUTEX, nullptr);
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
  WaitForSnapshots ();

  if (parent != nullptr)
    {
      LOG (INFO) << "Ending snapshot read transaction";
      PrepareRo ("ROLLBACK").Execute ();
    }

  if (sqliteSnapshot != nullptr && ownsSqliteSnapshot)
    {
      sqlite3_snapshot_free (sqliteSnapshot);
      sqliteSnapshot = nullptr;
    }

  ClearStatementCache ();

  CHECK (db != nullptr);
  const int rc = sqlite3_close (db);
  if (rc != SQLITE_OK)
    LOG (ERROR) << "Failed to close SQLite database";

  if (parent != nullptr)
    parent->UnrefSnapshot ();
}

SQLiteDatabase::CachedStatement::~CachedStatement ()
{
  CHECK (!used.test_and_set ()) << "Cached statement is still in use";

  /* sqlite3_finalize returns the error code corresponding to the last
     evaluation of the statement, not an error code "about" finalising it.
     Thus we want to ignore it here.  */
  sqlite3_finalize (stmt);
}

void
SQLiteDatabase::SetReadonlySnapshot (const SQLiteDatabase& p,
                                     sqlite3_snapshot* atSnap)
{
  CHECK (parent == nullptr && sqliteSnapshot == nullptr);
  parent = &p;
  LOG (INFO) << "Starting read transaction for snapshot";

  /* We need to exit auto-commit mode and start a deferred read transaction
     for the entire lifespan of this snapshot (it is ROLLBACK'ed in the
     destructor).  */
  PrepareRo ("BEGIN").Execute ();

  /* If we want to anchor the transaction to a particular snapshot
     from the parent database, open it now.  */
  if (atSnap != nullptr)
    CHECK_EQ (sqlite3_snapshot_open (db, "main", atSnap), SQLITE_OK)
        << "Failed to open parent snapshot position";

  /* Get a WAL snapshot for this exact state of the database.  This is used
     for getting future child snapshots of this database, which should be
     exact "copies".  This locks in the actual WAL position and starts the
     read transaction deferred above.  */
  CHECK (IsWalMode ()) << "Read-only snapshots only work in WAL mode";
  if (atSnap == nullptr)
    {
      /* Fresh snapshot from the live database: ask SQLite for the current
         WAL position.  This may fail, for instance if there is no physical
         WAL file on disk yet.  In that case, we cannot record a WAL token;
         the snapshot itself is valid and can be used, but it cannot be
         "copied" to child snapshots.  */
      const int sqliteRes = sqlite3_snapshot_get (db, "main", &sqliteSnapshot);
      if (sqliteRes != SQLITE_OK)
        {
          LOG (WARNING) << "Failed to record SQLite snapshot: " << sqliteRes;
          sqliteSnapshot = nullptr;
        }
      ownsSqliteSnapshot = true;
    }
  else
    {
      /* This connection was opened at an existing snapshot via
         sqlite3_snapshot_open.  sqlite3_snapshot_get does not work after
         snapshot_open on the same connection.  Instead, we store a non-owning
         pointer to the parent's snapshot token directly: the parent is
         guaranteed to outlive this connection by the snapshots reference
         count, so the pointer remains valid for as long as we need it.  */
      sqliteSnapshot = atSnap;
      ownsSqliteSnapshot = false;
    }

  /* Make sure to start a read transaction, in case we didn't do so yet
     with the sqlite3_snapshot_get call, which may not have happened
     or succeeded in all code paths.  */
  auto stmt = PrepareRo ("SELECT COUNT(*) FROM `sqlite_master`");
  CHECK (stmt.Step ());
  CHECK (!stmt.Step ());
}

void
SQLiteDatabase::ClearStatementCache ()
{
  std::lock_guard<std::mutex> lock(mutPreparedStatements);
  preparedStatements.clear ();
}

namespace
{

/**
 * Callback for sqlite3_exec that expects not to be called.
 */
int
ExpectNoResult (void* data, int columns, char** strs, char** names)
{
  LOG (FATAL) << "Expected no result from DB query";
}

} // anonymous namespace

void
SQLiteDatabase::Execute (const std::string& sql)
{
  AccessDatabase ([&sql] (sqlite3* h)
    {
      CHECK_EQ (sqlite3_exec (h, sql.c_str (), &ExpectNoResult,
                              nullptr, nullptr),
                SQLITE_OK);
    });
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

  /* First see if there is already an entry in our cache that
     we are free to use (because it is not yet in use).  */
  {
    std::lock_guard<std::mutex> lock(mutPreparedStatements);
    auto range = preparedStatements.equal_range (sql);
    for (auto it = range.first; it != range.second; ++it)
      if (!it->second->used.test_and_set ())
        {
          VLOG (2) << "Reusing cached SQL statement at " << it->second.get ();
          CHECK_EQ (sqlite3_clear_bindings (it->second->stmt), SQLITE_OK);

          auto res = Statement (*this, *it->second);
          res.Reset ();

          return res;
        }
  }

  /* If there was no matching (or free) statement, create a new one.  We can
     prepare it without holding mutPreparedStatements (but we need to lock
     before inserting into the map of course).  */

  sqlite3_stmt* stmt = nullptr;
  CHECK_EQ (sqlite3_prepare_v2 (db, sql.c_str (), sql.size () + 1,
                                &stmt, nullptr),
            SQLITE_OK)
      << "Failed to prepare SQL statement";

  auto entry = std::make_unique<CachedStatement> (stmt);
  entry->used.test_and_set ();
  Statement res(*this, *entry);

  VLOG (2)
      << "Created new SQL statement cache entry " << entry.get ()
      << " for:\n" << sql;

  std::lock_guard<std::mutex> lock(mutPreparedStatements);
  preparedStatements.emplace (sql, std::move (entry));

  return res;
}

void
SQLiteDatabase::WaitForSnapshots ()
{
  std::unique_lock<std::mutex> lock(mutSnapshots);
  LOG_IF (INFO, snapshots > 0)
      << "Waiting for outstanding snapshots to be finished...";
  waitingForSnapshots = true;
  while (snapshots > 0)
    cvSnapshots.wait (lock);
  waitingForSnapshots = false;
}

std::unique_ptr<SQLiteDatabase>
SQLiteDatabase::GetSnapshot () const
{
  if (!IsWalMode ())
    {
      LOG (WARNING) << "Snapshot is not possible for non-WAL database";
      return nullptr;
    }

  /* If this is a snapshot itself but has no SQLite WAL snapshot token, we
     cannot produce a copy of it.  */
  if (parent != nullptr && sqliteSnapshot == nullptr)
    {
      LOG (WARNING) << "Snapshot without WAL snapshot token cannot be copied";
      return nullptr;
    }

  std::lock_guard<std::mutex> lock(mutSnapshots);
  if (waitingForSnapshots)
    {
      LOG (WARNING) << "Waiting for snapshots to close, can't open new one";
      return nullptr;
    }
  ++snapshots;

  auto res = std::make_unique<SQLiteDatabase> (filename, SQLITE_OPEN_READONLY);
  res->SetReadonlySnapshot (*this, sqliteSnapshot);

  return res;
}

void
SQLiteDatabase::UnrefSnapshot () const
{
  std::lock_guard<std::mutex> lock(mutSnapshots);
  CHECK_GT (snapshots, 0);
  --snapshots;
  if (snapshots == 0)
    cvSnapshots.notify_all ();
}

void
SQLiteDatabase::WalCheckpoint ()
{
  if (!IsWalMode ())
    {
      LOG_FIRST_N (WARNING, 1) << "Database is not in WAL mode";
      return;
    }

  WaitForSnapshots ();
  /* Make sure to clear also all prepared statements, so that the
     database does not consider some operations still in progress
     that might contradict the WAL truncation.  */
  ClearStatementCache ();

  /* In theory, the code above ensures that we are good to do a checkpoint,
     with no existing open locks on the database.  However, in some usage
     scenarios, external processes might read the database file directly,
     and we have no control over that (but want to support these cases).

     For them, the WAL checkpointing might fail with SQLITE_BUSY.  In this
     case (and only this case), we fail gracefully, and just don't checkpoint;
     this is not a critical issue, as we might be able to just checkpoint
     later, and it won't cause any immediate difference to the database.  */
  const int res = sqlite3_wal_checkpoint_v2 (db, nullptr,
                                             SQLITE_CHECKPOINT_TRUNCATE,
                                             nullptr, nullptr);
  switch (res)
    {
    case SQLITE_BUSY:
      LOG (WARNING)
          << "Failed to checkpoint WAL file,"
             " another process might be reading the database";
      break;
    case SQLITE_OK:
      LOG (INFO) << "Checkpointed and truncated WAL file successfully";
      break;
    default:
      LOG (FATAL) << "Error checkpointing the WAL file: " << res;
    }
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
SQLiteStorage::WaitForSnapshots ()
{
  db->WaitForSnapshots ();
}

void
SQLiteStorage::CloseDatabase ()
{
  CHECK (db != nullptr);
  WaitForSnapshots ();
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

void
SQLiteStorage::SetupSchema ()
{
  LOG (INFO) << "Setting up database schema if it does not exist yet";
  db->Execute (R"(
    CREATE TABLE IF NOT EXISTS `xayagame_current`
        (`key` TEXT PRIMARY KEY,
         `value` BLOB NOT NULL);
    CREATE TABLE IF NOT EXISTS `xayagame_undo`
        (`hash` BLOB PRIMARY KEY,
         `data` BLOB NOT NULL,
         `height` INTEGER NOT NULL);
  )");
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

  /* Check if a periodic checkpointing of the WAL file is due.  */
  if (FLAGS_xaya_sqlite_wal_truncate_ms > 0)
    {
      const auto intv
          = std::chrono::milliseconds (FLAGS_xaya_sqlite_wal_truncate_ms);
      if (lastWalCheckpoint + intv <= Clock::now ())
        {
          LOG (INFO) << "Attempting periodic WAL checkpointing...";
          lastWalCheckpoint = Clock::now ();
          db->WalCheckpoint ();
        }
    }
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
