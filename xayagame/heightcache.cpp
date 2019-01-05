// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "heightcache.hpp"

#include <glog/logging.h>

namespace xaya
{
namespace internal
{

StorageWithCachedHeight::StorageWithCachedHeight (StorageInterface& s,
                                                  const HeightCallback& cb)
  : hashToHeight(cb), storage(&s)
{
  CHECK (storage != nullptr);
}

void
StorageWithCachedHeight::SetCurrentGameStateWithHeight (
    const uint256& hash, const unsigned height, const GameStateData& data)
{
  storage->SetCurrentGameState (hash, data);

  hasHeight = true;
  cachedHeight = height;

  VLOG (1) << "Cached height for block " <<  hash.ToHex () << ": " << height;
}

bool
StorageWithCachedHeight::GetCurrentBlockHashWithHeight (uint256& hash,
                                                        unsigned& height) const
{
  if (!storage->GetCurrentBlockHash (hash))
    return false;

  bool retrievedHeight = false;
  if (!hasHeight)
    {
      LOG (INFO) << "No cached block height, retrieving for " << hash.ToHex ();
      cachedHeight = hashToHeight (hash);
      hasHeight = true;
      retrievedHeight = true;
    }

  CHECK (hasHeight);
  height = cachedHeight;

  if (crossCheck && !retrievedHeight)
    CHECK_EQ (cachedHeight, hashToHeight (hash)) << "Cached height is wrong";

  return true;
}

void
StorageWithCachedHeight::SetCurrentGameState (const uint256& hash,
                                              const GameStateData& data)
{
  LOG (FATAL) << "SetCurrentGameStateWithHeight has to be used";
}

} // namespace internal
} // namespace xaya
