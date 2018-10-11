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
MemoryStorage::Clear ()
{
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
  HeightAndUndoData heightAndData = {height, data};
  undoData.emplace (hash, std::move (heightAndData));
}

void
MemoryStorage::ReleaseUndoData (const uint256& hash)
{
  undoData.erase (hash);
}

void
MemoryStorage::PruneUndoData (const unsigned height)
{
  for (auto it = undoData.cbegin (); it != undoData.cend (); )
    if (it->second.height <= height)
      it = undoData.erase (it);
    else
      ++it;
}

} // namespace xaya
