# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the handling of pending moves by Mover.
"""

from mover import MoverTest

import time


class PendingMovesTest (MoverTest):

  # The same test can be run with either one- or two-socket mode.  It should
  # work with both.  We have two separate scripts that execute the test in
  # the two modes.
  def __init__ (self, zmqPending):
    super (PendingMovesTest, self).__init__ ()
    self.zmqPending = zmqPending

  def run (self):
    self.log.info ("Testing ZMQ pending mode '%s'..." % self.zmqPending)

    self.generate (101)
    self.move ("a", "k", 5)
    self.move ("b", "j", 5)
    self.generate (1)
    oldState = self.getGameState ()
    self.assertEqual (oldState, {"players": {
      "a": {"x": 0, "y": 1, "dir": "up", "steps": 4},
      "b": {"x": 0, "y": -1, "dir": "down", "steps": 4},
    }})

    # Do some pending moves.  They include both new players and the existing
    # player, and multiple updates per player in the mempool.
    self.move ("a", "h", 3)
    self.move ("c", "l", 1)
    self.move ("c", "k", 2)
    time.sleep (0.1)
    oldPending = self.getPendingState ()
    self.assertEqual (oldPending, {
      "a":
        {
          "dir": "left",
          "steps": 3,
          "target": {"x": -3, "y": 1},
        },
      "c":
        {
          "dir": "up",
          "steps": 2,
          "target": {"x": 0, "y": 2},
        }
    })

    # Mine a block, which we will detach later and then reorg
    # back to the chain.  For now, this should clear the mempool.
    self.generate (1)
    reorgBlock = self.rpc.xaya.getbestblockhash ()
    newState = self.getGameState ()
    self.assertEqual (newState, {"players": {
      "a": {"x": -1, "y": 1, "dir": "left", "steps": 2},
      "b": {"x": 0, "y": -2, "dir": "down", "steps": 3},
      "c": {"x": 0, "y": 1, "dir": "up", "steps": 1},
    }})
    self.assertEqual (self.getPendingState (), {})

    # Detach the last two blocks.  This should put back the moves.
    self.rpc.xaya.invalidateblock (reorgBlock)
    self.expectGameState (oldState)
    self.assertEqual (self.getPendingState (), oldPending)

    # Mine one more block, which should clear the mempool again.
    self.generate (1)
    self.assertEqual (self.getPendingState (), {})

    # Reorg back to the long chain.  Note that this is something that may
    # in theory fail with the two-socket mode, because the re-added moves
    # may be processed after attaching the new tip.  But it seems to work
    # fine for now.
    self.rpc.xaya.reconsiderblock (reorgBlock)
    self.expectGameState (newState)
    self.assertEqual (self.getPendingState (), {})

    # Start the game daemon with pending tracking disabled.
    self.log.info ("Restarting game daemon without pending tracking...")
    self.stopGameDaemon ()
    self.startGameDaemon (extraArgs=["--nopending_moves"])
    self.expectError (-32603, ".*pending moves are not tracked.*",
                      self.getPendingState)
