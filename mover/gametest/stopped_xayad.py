#!/usr/bin/env python
# Copyright (C) 2018 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

import time

"""
Tests how the moverd reacts if xayad is stopped and restarted intermittantly.
"""


class StoppedXayadTest (MoverTest):

  def run (self):
    self.generate (101)
    self.expectGameState ({"players": {}})

    self.move ("a", "k", 2)
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 0, "y": 1, "dir": "up", "steps": 1},
    }})

    # Stop and restart the Xaya Core daemon.  This should not impact the game
    # daemon, at least not as long as it does not try to send a JSON-RPC
    # message while xayad is down.  The ZMQ subscription should be back up
    # again automatically.
    self.log.info ("Restarting Xaya daemon")
    self.stopXayaDaemon ()
    self.startXayaDaemon ()

    # Track the game again and sleep a short time, which ensures that the
    # ZMQ subscription has indeed had time to catch up again.
    self.rpc.xaya.trackedgames ("add", "mv")
    time.sleep (0.1)

    # Mine another block and verify that the game updates.
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 0, "y": 2},
    }})


if __name__ == "__main__":
  StoppedXayadTest ().main ()
