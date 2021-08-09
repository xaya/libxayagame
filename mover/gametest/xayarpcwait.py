#!/usr/bin/env python3
# Copyright (C) 2018-2021 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the --xaya_rpc_wait feature of mover (and thus the default main).
"""

from mover import MoverTest

import time


class XayaRpcWaitTest (MoverTest):

  def run (self):
    self.mainLogger.info ("Sending moves with shutdown GSP...")
    self.stopGameDaemon ()
    self.generate (101)
    self.move ("a", "k", 2)
    self.generate (1)

    self.mainLogger.info ("Stopping Xaya Core and starting the GSP...")
    self.xayanode.stop ()
    self.startGameDaemon (extraArgs=["--xaya_rpc_wait"], wait=False)

    self.mainLogger.info ("Starting Xaya Core as well to sync up...")
    self.xayanode.start ()
    self.rpc.xaya = self.xayanode.rpc
    time.sleep (1)
    self.expectGameState ({"players": {
      "a": {"x": 0, "y": 1, "dir": "up", "steps": 1},
    }})


if __name__ == "__main__":
  XayaRpcWaitTest ().main ()
