// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqlitegame.hpp"

#include "sqlitestorage.hpp"

#include <glog/logging.h>

#include <cstring>

namespace xaya
{

namespace
{

/** Keyword string for the initial game state.  */
constexpr const char* INITIAL_STATE = "initial";

/** Prefix for the block hash "game state" keywords.  */
constexpr const char* BLOCKHASH_STATE = "block ";

} // anonymous namespace

/**
 * Definition of SQLiteGame::Storage.  It is a basic subclass of SQLiteStorage
 * that has a reference to the SQLiteGame, so that it can call SetupSchema
 * there when the database is opened.
 */
class SQLiteGame::Storage : public SQLiteStorage
{

private:

  /** Reference to the SQLiteGame that is using this instance.  */
  SQLiteGame& game;

  /**
   * Set to true when we have a currently open transaction.  This is used to
   * verify that BeginTransaction is not called in a nested way.  (Savepoints
   * would in theory support that, but we exclude it nevertheless.)
   */
  bool startedTransaction = false;

  void
  BeginTransaction ()
  {
    CHECK (!startedTransaction);
    startedTransaction = true;
    StepWithNoResult (PrepareStatement ("SAVEPOINT `xayagame-sqlitegame`"));
  }

  void
  CommitTransaction ()
  {
    StepWithNoResult (PrepareStatement ("RELEASE `xayagame-sqlitegame`"));
    CHECK (startedTransaction);
    startedTransaction = false;
  }

  void
  RollbackTransaction ()
  {
    StepWithNoResult (PrepareStatement ("ROLLBACK TO `xayagame-sqlitegame`"));
    CHECK (startedTransaction);
    startedTransaction = false;
  }

  /**
   * Verifies that the database state corresponds to the given "current state"
   * from libxayagame.  The function also makes sure to call InitialiseState
   * if the passed-in state is the initial-keyword and the database's state
   * has not yet been initialised.
   */
  void EnsureCurrentState (const GameStateData& state);

  friend class SQLiteGame;

protected:

  void
  SetupSchema () override
  {
    SQLiteStorage::SetupSchema ();

    const int rc = sqlite3_exec (GetDatabase (), R"(
      CREATE TABLE IF NOT EXISTS `xayagame_gamevars`
          (`onlyonerow` INTEGER PRIMARY KEY,
           `gamestate_initialised` INTEGER);
      INSERT OR IGNORE INTO `xayagame_gamevars`
          (`onlyonerow`, `gamestate_initialised`) VALUES (1, 0);
    )", nullptr, nullptr, nullptr);
    CHECK_EQ (rc, SQLITE_OK) << "Failed to set up SQLiteGame's database schema";

    /* Since we use the session extension to handle rollbacks, only the main
       database should be used.  To enforce this (at least partially), disallow
       any attached databases.  */
    sqlite3_limit (GetDatabase (), SQLITE_LIMIT_ATTACHED, 0);
    LOG (INFO) << "Set allowed number of attached databases to zero";

    game.SetupSchema (GetDatabase ());
  }

public:

  explicit Storage (SQLiteGame& g, const std::string& f)
    : SQLiteStorage (f), game(g)
  {}

};

void
SQLiteGame::Storage::EnsureCurrentState (const GameStateData& state)
{
  VLOG (1) << "Ensuring current database matches game state: " << state;

  /* In any case, state-based methods of GameLogic are only ever called when
     there is already a "current state" in the storage.  */
  uint256 hash;
  CHECK (GetCurrentBlockHash (hash));
  const std::string hashHex = hash.ToHex ();

  /* Handle the case of a regular block hash (no initial state).  */
  const size_t prefixLen = std::strlen (BLOCKHASH_STATE);
  if (state.substr (0, prefixLen) == BLOCKHASH_STATE)
    {
      CHECK (hashHex == state.substr (prefixLen))
          << "Current best block in the database (" << hashHex
          << ") does not match claimed current game state";
      return;
    }

  /* Verify initial state.  */
  CHECK (state == INITIAL_STATE) << "Unexpected game state value: " << state;
  unsigned height;
  std::string initialHashHex;
  game.GetInitialStateBlock (height, initialHashHex);
  CHECK (hashHex == initialHashHex)
      << "Current best block in the database (" << hashHex
      << ") does not match the game's initial block " << initialHashHex;

  /* Check if the state has already been initialised.  */
  auto* stmt = PrepareStatement (R"(
    SELECT `gamestate_initialised` FROM `xayagame_gamevars`
  )");
  CHECK_EQ (sqlite3_step (stmt), SQLITE_ROW)
      << "Failed to fetch result for from xayagame_gamevars";
  const int initialised = sqlite3_column_int (stmt, 0);
  StepWithNoResult (stmt);

  /* If it has not yet been initialised, do so now.  */
  if (initialised != 0)
    {
      VLOG (1) << "Initial state has already been set in the DB";
      return;
    }

  LOG (INFO) << "Setting initial state in the DB";
  StepWithNoResult (PrepareStatement ("SAVEPOINT `xayagame-stateinit`"));
  try
    {
      game.InitialiseState (GetDatabase ());
      StepWithNoResult (PrepareStatement (R"(
        UPDATE `xayagame_gamevars` SET `gamestate_initialised` = 1
      )"));
      StepWithNoResult (PrepareStatement ("RELEASE `xayagame-stateinit`"));
      LOG (INFO) << "Initialised the DB state successfully";
    }
  catch (...)
    {
      LOG (ERROR) << "Initialising state failed, rolling back the DB change";
      StepWithNoResult (PrepareStatement ("ROLLBACK TO `xayagame-stateinit`"));
      throw;
    }
}

SQLiteGame::SQLiteGame (const std::string& f)
  : database(std::make_unique<Storage> (*this, f))
{}

/* The destructor cannot be =default'ed in the header, since Storage is
   an incomplete type at that stage.  */
SQLiteGame::~SQLiteGame () = default;

void
SQLiteGame::SetupSchema (sqlite3* db)
{
  /* Nothing needs to be set up here, but subclasses probably do some setup
     in an overridden method.  The set up of the schema we need for SQLiteGame
     is done in Storage::SetupSchema already before calling here.  */
}

StorageInterface*
SQLiteGame::GetStorage ()
{
  return database.get ();
}

GameStateData
SQLiteGame::GetInitialState (unsigned& height, std::string& hashHex)
{
  GetInitialStateBlock (height, hashHex);
  return INITIAL_STATE;
}

namespace
{

/**
 * Helper class wrapping aroung sqlite3_session and using RAII to ensure
 * proper management of its lifecycle.
 */
class SQLiteSession
{

private:

  /** The underlying sqlite3_session handle.  */
  sqlite3_session* session = nullptr;

public:

  /**
   * Construct a new session, monitoring the "main" database on the given
   * DB connection.
   */
  explicit SQLiteSession (sqlite3* db)
  {
    VLOG (1) << "Starting SQLite session to record undo data";

    CHECK_EQ (sqlite3session_create (db, "main", &session), SQLITE_OK)
        << "Failed to start SQLite session";
    CHECK (session != nullptr);
    CHECK_EQ (sqlite3session_attach (session, nullptr), SQLITE_OK)
        << "Failed to attach all tables to the SQLite session";
  }

  ~SQLiteSession ()
  {
    if (session != nullptr)
      sqlite3session_delete (session);
  }

  /**
   * Extracts the current changeset of the session as UndoData string.
   */
  UndoData
  ExtractChangeset ()
  {
    VLOG (1) << "Extracting recorded undo data from SQLite session";
    CHECK (session != nullptr);

    int changeSize;
    void* changeBytes;
    CHECK_EQ (sqlite3session_changeset (session, &changeSize, &changeBytes),
              SQLITE_OK)
        << "Failed to extract current session changeset";

    UndoData result(static_cast<const char*> (changeBytes), changeSize);
    sqlite3_free (changeBytes);

    return result;
  }

};

} // anonymous namespace

GameStateData
SQLiteGame::ProcessForward (const GameStateData& oldState,
                            const Json::Value& blockData,
                            UndoData& undo)
{
  database->EnsureCurrentState (oldState);

  SQLiteSession session(database->GetDatabase ());
  UpdateState (database->GetDatabase (), blockData);
  undo = session.ExtractChangeset ();

  return BLOCKHASH_STATE + blockData["block"]["hash"].asString ();
}

namespace
{

/**
 * Conflict resolution function for sqlite3changeset_apply that simply
 * tells to abort the transaction.  (If all goes correct, then conflicts should
 * never happen as we simply rollback *the last* change and are not "merging"
 * changes in any way.)
 */
int
AbortOnConflict (void* ctx, const int conflict, sqlite3_changeset_iter* it)
{
  LOG (ERROR) << "Changeset application has a conflict of type " << conflict;
  return SQLITE_CHANGESET_ABORT;
}

/**
 * Utility class to manage an inverted changeset (based on undo data
 * representing an original one).  The main use of the class is to manage
 * the associated memory using RAII.
 */
class InvertedChangeset
{

private:

  /** Size of the inverted changeset.  */
  int size;
  /** Buffer holding the data for the inverted changeset.  */
  void* data = nullptr;

public:

  /**
   * Constructs the changeset by inverting the UndoData that represents
   * the original "forward" changeset.
   */
  explicit InvertedChangeset (const UndoData& undo)
  {
    CHECK_EQ (sqlite3changeset_invert (undo.size (), &undo[0], &size, &data),
              SQLITE_OK)
        << "Failed to invert SQLite changeset";
  }

  ~InvertedChangeset ()
  {
    sqlite3_free (data);
  }

  /**
   * Applies the inverted changeset to the database handle.  If conflicts
   * appear, the transaction is aborted and the function CHECK-fails.
   */
  void
  Apply (sqlite3* db)
  {
    CHECK_EQ (sqlite3changeset_apply (db, size, data, nullptr,
                                      &AbortOnConflict, nullptr),
              SQLITE_OK)
        << "Failed to apply undo changeset";
  }

};

} // anonymous namespace

GameStateData
SQLiteGame::ProcessBackwards (const GameStateData& newState,
                              const Json::Value& blockData,
                              const UndoData& undo)
{
  database->EnsureCurrentState (newState);

  /* Note that the undo data holds the *forward* changeset, not the inverted
     one.  Thus we have to invert it here before applying.  It might seem
     more intuitive for the undo data to already hold the inverted changeset,
     but as it is expected that most undo data values are never actually
     used to roll any changes back, it is more efficient to do the inversion
     only when actually needed.  */

  InvertedChangeset changeset(undo);
  changeset.Apply (database->GetDatabase ());

  return BLOCKHASH_STATE + blockData["block"]["parent"].asString ();
}

void
SQLiteGame::BeginTransaction ()
{
  database->BeginTransaction ();
}

void
SQLiteGame::CommitTransaction ()
{
  database->CommitTransaction ();
}

void
SQLiteGame::RollbackTransaction ()
{
  database->RollbackTransaction ();
}

Json::Value
SQLiteGame::GameStateToJson (const GameStateData& state)
{
  database->EnsureCurrentState (state);
  return GetStateAsJson (database->GetDatabase ());
}

} // namespace xaya
