-- Copyright (C) 2020 The Xaya developers
-- Distributed under the MIT software license, see the accompanying
-- file COPYING or http://www.opensource.org/licenses/mit-license.php.

-- All the assets that have been created yet.  This table is append only:
-- New assets can be created (if the user/asset combination does not yet
-- exist), but once created, the data is immutable.
CREATE TABLE IF NOT EXISTS `assets` (

  -- The minting user's name.
  `minter` TEXT NOT NULL,

  -- The asset's name (within all assets of the user).
  `asset` TEXT NOT NULL,

  -- The associated data string (which can e.g. be a hash or IPFS link
  -- to some metadata about this asset).
  `data` TEXT NULL,

  PRIMARY KEY (`minter`, `asset`)

);

-- All non-zero balances of assets that users have.
CREATE TABLE IF NOT EXISTS `balances` (

  -- The user's name who owns the balance.
  `name` TEXT NOT NULL,

  -- The asset this is about.
  `minter` TEXT NOT NULL,
  `asset` TEXT NOT NULL,

  -- The balance.  It is always larger than zero; if it reaches
  -- zero, then the entry is removed.
  `balance` INTEGER NOT NULL,

  PRIMARY KEY (`name`, `minter`, `asset`)

);

-- Allow efficient retrieval of all balances of a given asset.
CREATE INDEX IF NOT EXISTS `balances_by_asset`
    ON `balances` (`minter`, `asset`);
