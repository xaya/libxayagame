// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "storage.hpp"

#include <glog/logging.h>

namespace xaya
{

void
StorageInterface::Initialise ()
{
  /* Nothing is done here, but can be overridden by subclasses.  */
}

void
StorageInterface::BeginTransaction ()
{
  /* Nothing is done in the default implementation.  */
}

void
StorageInterface::CommitTransaction ()
{
  /* Nothing is done in the default implementation.  */
}

void
StorageInterface::RollbackTransaction ()
{
  /* Nothing is done in the default implementation.  */
}

void
MemoryStorage::Clear ()
{
  CHECK (!startedTxn);

  hasState = false;
  undoData.clear ();
}

bool
MemoryStorage::GetCurrentBlockHash (uint256& hash) const
{
  if (!hasState)
    return false;

  hash = currentBlock;
  return true;
}

GameStateData
MemoryStorage::GetCurrentGameState () const
{
  CHECK (hasState);
  return currentState;
}

void
MemoryStorage::SetCurrentGameState (const uint256& hash,
                                    const GameStateData& data)
{
  CHECK (startedTxn);

  hasState = true;
  currentBlock = hash;
  currentState = data;
}

bool
MemoryStorage::GetUndoData (const uint256& hash, UndoData& data) const
{
  const auto mit = undoData.find (hash);
  if (mit == undoData.end ())
    return false;

  data = mit->second.data;
  return true;
}

void
MemoryStorage::AddUndoData (const uint256& hash,
                            const unsigned height, const UndoData& data)
{
  CHECK (startedTxn);

  HeightAndUndoData heightAndData = {height, data};
  undoData.emplace (hash, std::move (heightAndData));
}

void
MemoryStorage::ReleaseUndoData (const uint256& hash)
{
  CHECK (startedTxn);
  undoData.erase (hash);
}

void
MemoryStorage::PruneUndoData (const unsigned height)
{
  CHECK (startedTxn);

  for (auto it = undoData.cbegin (); it != undoData.cend (); )
    if (it->second.height <= height)
      it = undoData.erase (it);
    else
      ++it;
}

void
MemoryStorage::BeginTransaction ()
{
  CHECK (!startedTxn);
  startedTxn = true;
}

void
MemoryStorage::CommitTransaction ()
{
  CHECK (startedTxn);
  startedTxn = false;
}

void
MemoryStorage::RollbackTransaction ()
{
  CHECK (startedTxn);
  startedTxn = false;

  LOG (WARNING) << "Memory storage is not capable of rolling back transactions";
}

} // namespace xaya
