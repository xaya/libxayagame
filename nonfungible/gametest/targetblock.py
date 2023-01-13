#!/usr/bin/env python3

# Copyright (C) 2023 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the "target block" feature in the GSP.  This is the main integration
test for the feature in libxayagame itself as well.
"""

from nftest import NonFungibleTest

import time


class TargetBlockTest (NonFungibleTest):

  def getStateAtTarget (self, blk):
    """
    Wait for the GSP to be in at-target state and then return
    the associated game state.
    """

    while True:
      state = self.rpc.game.getcurrentstate ()
      if state["state"] == "at-target":
        self.assertEqual (state["blockhash"], blk)
        return state["gamestate"]
      time.sleep (0.1)

  def run (self):
    # Stop the game daemon, so we can generate an existing chain against
    # which we then test the target-block syncing.
    self.mainLogger.info ("Pre-generating blockchain without GSP attached...")
    self.stopGameDaemon ()

    self.sendMove ("domob", [
      {"m": {"a": "foo", "n": 10}},
    ])
    self.generate (10)
    blk1, _ = self.env.getChainTip ()

    self.sendMove ("domob", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 1, "r": "andy"}},
    ])
    self.generate (2)
    blk2, _ = self.env.getChainTip ()

    self.sendMove ("domob", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 2, "r": "andy"}},
    ])
    self.generate (1)

    # Start with an explicit target hash right away.
    self.mainLogger.info ("Starting GSP with target hash...")
    self.startGameDaemon (extraArgs=["--target_block=%s" % blk1])
    self.assertEqual (self.getStateAtTarget (blk1), [
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 10},
      },
    ])

    # Test setting the target block by RPC.
    self.mainLogger.info ("Testing target block by RPC...")
    self.rpc.game._notify.settargetblock (blk2)
    time.sleep (0.1)
    self.assertEqual (self.getStateAtTarget (blk2), [
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 9, "andy": 1},
      },
    ])
    self.rpc.game._notify.settargetblock ("")
    self.expectGameState ([
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 7, "andy": 3},
      },
    ])
    self.sendMove ("domob", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 3, "r": "andy"}},
    ])
    self.generate (1)
    self.expectGameState ([
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 4, "andy": 6},
      },
    ])

    # Test a target state in the past.
    # FIXME:  For now, the GSP does not explicitly trigger ZMQ notifications
    # to the target state.  So this only works if we trigger detaches
    # externally.
    self.rpc.game._notify.settargetblock (blk2)
    self.rpc.xaya.invalidateblock (blk1)
    self.assertEqual (self.getStateAtTarget (blk2), [
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 9, "andy": 1},
      },
    ])


if __name__ == "__main__":
  TargetBlockTest ().main ()
