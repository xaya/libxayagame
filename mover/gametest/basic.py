#!/usr/bin/env python
# Copyright (C) 2018 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

"""
Tests basic game operation with some moves and the resulting game state.
"""


class BasicTest (MoverTest):

  def run (self):
    self.generate (101)
    self.expectGameState ({"players": {}})

    self.move ("a", "k", 2)
    self.move ("b", "y", 1)

    # Without confirming the transactions, the game state should not be changed.
    self.expectGameState ({"players": {}})

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

    # Send an invalid move, which should be ignored.
    self.sendMove ("a", {"d": "h", "n": 1000001})
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 2, "y": 1},
      "b": {"x": -1, "y": 1},
    }})


if __name__ == "__main__":
  BasicTest ().main ()
