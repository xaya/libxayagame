#!/usr/bin/env python3
# Copyright (C) 2018-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

"""
Tests that mover also works fine if the LMDB map has to be resized.
"""

# Regexp for the log that is printed when we resize.
RESIZED_LMDB = 'Resizing LMDB map'


class LMDBResizeTest (MoverTest):

  def run (self):
    self.log.info ("Restarting with LMDB storage...")
    self.stopGameDaemon ()
    self.startGameDaemon (extraArgs=["--storage_type=lmdb"])

    self.generate (101)

    # To cause a resize, we have to generate undo data of more than 1 MiB
    # in size.  The undo data for a block contains at least the names of
    # all players that were updated.  So by moving around a couple of players
    # with long names, we can achieve this.  Note that we make sure to have
    # less than 25 players, so that we can be sure to not run into the
    # mempool-chain limit with the name_update's.

    players = []
    totalLen = 0
    for c in "abcdefghijklmnopqrst":
      cur = c * 250
      players.append (cur)
      totalLen += len (cur)

    blocks = 250
    assert blocks * totalLen > (1 << 20)

    for i in range (blocks):
      if (i + 1) % 10 == 0:
        self.mainLogger.info (
            "Processing block %d of %d..." % ((i + 1), blocks))
      for p in players:
        self.move (p, "k", 1)
      self.generate (1)

    expectedPlayers = {}
    for p in players:
      expectedPlayers[p] = {"x": 0, "y": blocks}
    self.expectGameState ({"players": expectedPlayers})

    self.stopGameDaemon ()
    assert self.gamenode.logMatches (RESIZED_LMDB)


if __name__ == "__main__":
  LMDBResizeTest ().main ()
