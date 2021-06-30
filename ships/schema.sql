-- Copyright (C) 2019-2021 The Xaya developers
-- Distributed under the MIT software license, see the accompanying
-- file COPYING or http://www.opensource.org/licenses/mit-license.php.

-- Statistics about won/lost games by Xaya name.
CREATE TABLE IF NOT EXISTS `game_stats` (

  -- The p/ name for an entry.
  `name` TEXT PRIMARY KEY,

  -- The number of won games.
  `won` INTEGER NOT NULL,

  -- The number of lost games.
  `lost` INTEGER NOT NULL

);

-- Extra data about open channels, in addition to what the core game-channel
-- library holds and uses.
CREATE TABLE IF NOT EXISTS `channel_extradata` (

  -- The channel ID this entry is for.
  `id` BLOB PRIMARY KEY,

  -- The height when the channel was created.
  `createdheight` INTEGER NOT NULL,

  -- Number of participants in the channel.
  `participants` INTEGER NOT NULL

);

-- Allows to query for channels that were created some time ago and
-- have not yet gotten a second participant.
CREATE INDEX IF NOT EXISTS `channel_extradata_by_height_and_participants`
  ON `channel_extradata` (`createdheight`, `participants`);
