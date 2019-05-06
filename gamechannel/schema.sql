-- Copyright (C) 2019 The Xaya developers
-- Distributed under the MIT software license, see the accompanying
-- file COPYING or http://www.opensource.org/licenses/mit-license.php.

-- The data for all open game channels that is part of the global game state.
CREATE TABLE IF NOT EXISTS `xayagame_game_channels` (

  -- The ID of the channel, which is typically the txid that created it.
  `id` BLOB PRIMARY KEY,

  -- The current metadata (mainly list of participants), as a serialised
  -- ChannelMetadata protocol buffer instance.
  `metadata` BLOB NOT NULL,

  -- The latest board state that is put on-chain as encoded data.
  `state` BLOB NOT NULL,

  -- If there is a dispute open (based on the current proven state), then
  -- the block height when it was filed.
  `disputeheight` INTEGER NULL

);

-- We need to look up disputed channels by height, so that we can iterate
-- over all timed out channels and process them.
CREATE INDEX IF NOT EXISTS `xayagame_game_channels_by_disputeheight`
    ON `xayagame_game_channels` (`disputeheight`);
