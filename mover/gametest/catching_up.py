#!/usr/bin/env python3
# Copyright (C) 2018-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

import time

"""
Tests catching up of the game if it gets out of sync.
"""


class CatchingUpTest (MoverTest):

  def run (self):
    self.generate (101)
    self.expectGameState ({"players": {}})
    oldState = self.rpc.game.getcurrentstate ()

    # Disable tracking of the game, so it won't get notified about changes
    # we make and get out of sync.
    self.log.info ("Untracking g/mv")
    self.rpc.xaya.trackedgames ("remove", "mv")
    self.move ("a", "k", 2)
    self.generate (1)

    # We cannot use expectGameState / getGameState, as that would wait for
    # the game to catch up (which it doesn't do).  So just sleep a short time
    # and hope this is good enough the ensure that the game would have caught
    # up if there were some issue with the test.
    time.sleep (0.1)
    state = self.rpc.game.getcurrentstate ()
    assert state == oldState

    # Start to track the game again and mine another block.  This should make
    # the game catch up.
    self.log.info ("Tracking g/mv again")
    self.rpc.xaya.trackedgames ("add", "mv")
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 0, "y": 2},
    }})

    # Now stop the game daemon and let it catch up later when restarted.  Note
    # that this leads to a sync from scratch with the MemoryStorage, but that
    # doesn't matter here.
    self.log.info ("Stopping game daemon")
    self.stopGameDaemon ()
    self.move ("a", "l", 1)
    self.generate (1)

    # Start the game daemon again and check that it catches up correctly.
    self.log.info ("Restarting game daemon")
    self.startGameDaemon ()
    self.expectGameState ({"players": {
      "a": {"x": 1, "y": 2},
    }})


if __name__ == "__main__":
  CatchingUpTest ().main ()
