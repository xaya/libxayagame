#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests what happens if name_update's triggered from a channel daemon fail.
"""

from shipstest import ShipsTest


class TxFailTest (ShipsTest):

  def run (self):
    myAddr = self.rpc.xaya.getnewaddress ()
    self.rpc.xaya.generatetoaddress (10, myAddr)
    self.generate (150)

    # Create a test channel with two participants.
    self.mainLogger.info ("Creating test channel...")
    channelId = self.openChannel (["foo", "bar"])

    # Start up the two channel daemons.
    self.mainLogger.info ("Starting channel daemons...")
    with self.runChannelDaemon (channelId, "foo") as foo, \
         self.runChannelDaemon (channelId, "bar") as bar:

      daemons = [foo, bar]

      self.mainLogger.info ("Running initialisation sequence...")
      foo.rpc._notify.setposition ("""
        xxxx..xx
        ........
        xxx.xxx.
        ........
        xx.xx.xx
        ........
        ........
        ........
      """)
      bar.rpc._notify.setposition ("""
        ........
        ........
        ........
        xxxx..xx
        ........
        xxx.xxx.
        ........
        xx.xx.xx
      """)
      _, state = self.waitForPhase (daemons, ["shoot"])

      # Make sure it is foo's turn.  If not, miss a shot with bar.
      if state["current"]["state"]["whoseturn"] == 1:
        bar.rpc._notify.shoot (row=7, column=0)
        _, state = self.waitForPhase (daemons, ["shoot"])
      self.assertEqual (state["current"]["state"]["whoseturn"], 0)

      # File a dispute with locked wallet.  That should just silently fail.
      self.mainLogger.info ("Trying dispute that fails...")
      self.lock ()
      bar.rpc._notify.filedispute ()
      self.expectPendingMoves ("bar", [])
      self.assertEqual (bar.getCurrentState ()["pending"], {})
      self.generate (1)
      state = bar.getCurrentState ()
      assert "dispute" not in state
      self.unlock ()

      # Let bar file a dispute against foo.
      self.mainLogger.info ("Filing a dispute whose resolution fails...")
      bar.rpc._notify.filedispute ()
      self.generate (1)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": False,
        "height": self.rpc.xaya.getblockcount (),
      })

      # Send a new move, but lock the wallet so that the resolution
      # transaction will not succeed.
      self.lock ()
      foo.rpc._notify.shoot (row=7, column=0)

      # Unlock the wallet.  Then the resolution transaction should be
      # retried when another block comes in.
      self.mainLogger.info ("Resolving it with unlocked wallet...")
      self.unlock ()
      self.expectPendingMoves ("foo", [])
      self.assertEqual (foo.getCurrentState ()["pending"], {})
      self.generate (1)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": True,
        "height": self.rpc.xaya.getblockcount () - 1,
      })
      self.expectPendingMoves ("foo", ["r"])
      self.generate (1)
      _, state = self.waitForPhase (daemons, ["shoot"])
      assert "dispute" not in state

      # Let foo lose the game.  We lock the wallet, so that the winner
      # statement of bar cannot get sent initially.
      self.mainLogger.info ("Winner statement fails...")
      self.lock ()
      foo.rpc._notify.revealposition ()
      self.waitForPhase (daemons, ["finished"])

      # Now unlock the wallet.  Then the next block should re-trigger an update
      # and we should get the move in.
      self.mainLogger.info ("Winner statement retrial succeeds...")
      self.unlock ()
      self.expectPendingMoves ("bar", [])
      self.generate (1)
      state = bar.getCurrentState ()
      self.assertEqual (state["current"]["state"]["parsed"]["winner"], 1)
      self.expectPendingMoves ("bar", ["w"])
      self.generate (1)
      state = foo.getCurrentState ()
      self.assertEqual (state["existsonchain"], False)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 0, "lost": 1},
          "bar": {"won": 1, "lost": 0},
        },
      })

  def generate (self, n):
    """
    Mines n blocks, but to an address not owned by the test wallet.
    This ensures that we keep control over locked/unlocked outputs as
    needed in this test, and not new outputs get added when mining blocks.
    """

    notMine = "cdpSgeapVR8ZgRkqA8zF3fDJ2NgaUqm2pu"
    self.rpc.xaya.generatetoaddress (n, notMine)

  def lock (self):
    """
    Locks all UTXO's in the wallet, so that no name_update transactions
    can be made temporarily.  This does not affect signing messages.
    """

    outputs = self.rpc.xaya.listunspent ()
    self.rpc.xaya.lockunspent (False, outputs)

  def unlock (self):
    """
    Unlocks all outputs in the wallet, so that name_update's can be done
    again successfully.
    """

    self.rpc.xaya.lockunspent (True)


if __name__ == "__main__":
  TxFailTest ().main ()
