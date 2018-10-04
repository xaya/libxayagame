#!/usr/bin/env python
# Copyright (C) 2018 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

"""
Tests basic operation with pruning enabled.

Note that this test mainly ensures that things still "work" with pruning
enabled.  It can't verify that data gets pruned, and it also can't verify
that not too much is pruned.  The latter is because even if too much gets
removed, things are fine as the game "just" resyncs from scratch.  However,
in that case an ERROR is logged, which can be noticed at least manually
when running the test.
"""


class PruningTest (MoverTest):

  def run (self):
    self.generate (101)
    self.expectGameState ({"players": {}})

    # Test that basic forward-processing works even with no kept blocks.
    self.setPruning (0)

    self.move ("a", "k", 2)
    self.move ("b", "y", 1)
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 0, "y": 1, "dir": "up", "steps": 1},
      "b": {"x": -1, "y": 1},
    }})

    self.move ("a", "l", 2)
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 1, "y": 1,  "dir": "right", "steps": 1},
      "b": {"x": -1, "y": 1},
    }})

    # Enable pruning while keeping at least one block, so that we can reorg.
    self.setPruning (1)

    txid = self.move ("a", "j", 1)
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 1, "y": 0},
      "b": {"x": -1, "y": 1},
    }})
    blk = self.rpc.xaya.getbestblockhash ()

    self.rpc.xaya.invalidateblock (blk)
    # The previous move of "a" should have been put back into the mempool.
    assert self.rpc.xaya.getrawmempool () == [txid]
    self.move ("b", "n", 1)
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 1, "y": 0},
      "b": {"x": 0, "y": 0},
    }})

  def setPruning (self, value):
    """
    Restarts the game daemon to change the pruning setting to the given value.
    """

    self.log.info ("Setting pruning to %d" % value)
    self.stopGameDaemon ()
    self.startGameDaemon (extraArgs=["--enable_pruning=%d" % value])


if __name__ == "__main__":
  PruningTest ().main ()
