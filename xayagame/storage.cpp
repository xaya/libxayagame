// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "storage.hpp"

#include <glog/logging.h>

namespace xaya
{

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

  data = mit->second;
  return true;
}

void
MemoryStorage::AddUndoData (const uint256& hash, const UndoData& data)
{
  undoData.emplace (hash, data);
}

void
MemoryStorage::ReleaseUndoData (const uint256& hash)
{
  undoData.erase (hash);
}

} // namespace xaya
