-- Copyright (C) 2019 The Xaya developers
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
