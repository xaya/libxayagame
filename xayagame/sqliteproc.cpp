// Copyright (C) 2022-2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sqliteproc.hpp"

#include "sqliteintro.hpp"

#include <xayautil/hash.hpp>

#include <glog/logging.h>

namespace xaya
{

/* ************************************************************************** */

SQLiteProcessor::~SQLiteProcessor ()
{
  /* If we had async processes started, they should have been closed
     already in Finish().  */
  CHECK (runner == nullptr);
}

void
SQLiteProcessor::StoreResult (SQLiteDatabase& db)
{
  db.Prepare ("SAVEPOINT `xayagame-processor`").Execute ();
  Store (db);
  db.Prepare ("RELEASE `xayagame-processor`").Execute ();
}

bool
SQLiteProcessor::ShouldRun (const Json::Value& blockData) const
{
  if (blockInterval == 0)
    return false;

  CHECK (blockData.isObject ());
  const auto& heightVal = blockData["height"];
  CHECK (heightVal.isUInt64 ());
  const uint64_t height = heightVal.asUInt64 ();

  return (height % blockInterval) == blockModulo;
}

void
SQLiteProcessor::SetupSchema (SQLiteDatabase& db)
{}

void
SQLiteProcessor::Finish (SQLiteDatabase& db)
{
  /* Make sure to wait for and store the result of any running process.  */
  if (runner != nullptr)
    {
      runner->join ();
      runner.reset ();
      StoreResult (db);
    }
}

void
SQLiteProcessor::Process (const Json::Value& blockData,
                          SQLiteDatabase& db,
                          std::shared_ptr<SQLiteDatabase> snapshot)
{
  const bool shouldRun = ShouldRun (blockData);

  /* If we have a finished thread, store its result.  Also if we start
     a run now, make sure to always wait for the previous one to be done.  */
  if (runner != nullptr && (shouldRun || !processing))
    {
      runner->join ();
      runner.reset ();
      StoreResult (db);
    }

  if (!shouldRun)
    return;

  CHECK (runner == nullptr);

  /* If we don't have a snapshot, run synchronously.  */
  if (snapshot == nullptr)
    {
      Compute (blockData, db);
      StoreResult (db);
      return;
    }

  /* We have a snapshot, on which we can run synchronous processing.  */
  runner = std::make_unique<std::thread> ([this, blockData, snapshot] ()
    {
      processing = true;
      Compute (blockData, *snapshot);
      processing = false;
    });
}

void
SQLiteProcessor::SetInterval (const uint64_t intv, const uint64_t modulo)
{
  blockInterval = intv;
  blockModulo = modulo;
}

/* ************************************************************************** */

void
SQLiteHasher::SetupSchema (SQLiteDatabase& db)
{
  SQLiteProcessor::SetupSchema (db);
  db.Execute (R"(
    CREATE TABLE IF NOT EXISTS `xayagame_statehashes`
        (`block` BLOB PRIMARY KEY,
         `hash` BLOB NOT NULL);
  )");
} 

std::set<std::string>
SQLiteHasher::GetTables (const SQLiteDatabase& db)
{
  return GetSqliteTables (db);
}

void
SQLiteHasher::Compute (const Json::Value& blockData, const SQLiteDatabase& db)
{
  const auto hashVal = blockData["hash"];
  CHECK (hashVal.isString ());
  CHECK (block.FromHex (hashVal.asString ()));

  VLOG (1) << "Computing game-state hash for block " << block.ToHex ();
  SHA256 hasher;
  WriteTables (hasher, db, GetTables (db));
  hash = hasher.Finalise ();
}

void
SQLiteHasher::Store (SQLiteDatabase& db)
{
  /* First check that if a hash exists already, it matches what we computed.
     Otherwise there is some kind of serious bug.  */
  uint256 existing;
  if (GetHash (db, block, existing))
    CHECK (existing == hash)
        << "Already stored game-state differs from computed for block "
        << block.ToHex ();

  auto stmt = db.Prepare (R"(
    INSERT OR IGNORE INTO `xayagame_statehashes`
      (`block`, `hash`) VALUES (?1, ?2)
  )");
  stmt.Bind (1, block);
  stmt.Bind (2, hash);
  stmt.Execute ();
}

bool
SQLiteHasher::GetHash (const SQLiteDatabase& db, const uint256& block,
                       uint256& hash) const
{
  auto stmt = db.PrepareRo (R"(
    SELECT `hash`
      FROM `xayagame_statehashes`
      WHERE `block` = ?1
  )");
  stmt.Bind (1, block);

  if (!stmt.Step ())
    return false;

  hash = stmt.Get<uint256> (0);
  CHECK (!stmt.Step ());
  return true;
}

/* ************************************************************************** */

} // namespace xaya
