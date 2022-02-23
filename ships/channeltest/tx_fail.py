#!/usr/bin/env python3
# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests what happens if name_update's triggered from a channel daemon fail.
"""

from shipstest import ShipsTest


class TxFailTest (ShipsTest):

  def run (self):
    self.generate (150)

    # Create a test channel with two participants.
    self.mainLogger.info ("Creating test channel...")
    channelId, addr = self.openChannel (["foo", "bar"])

    # Start up the two channel daemons.
    self.mainLogger.info ("Starting channel daemons...")
    with self.runChannelDaemon (channelId, "foo", addr[0]) as foo, \
         self.runChannelDaemon (channelId, "bar", addr[1]) as bar:

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
        with self.waitForTurnIncrease (daemons, 2):
          bar.rpc._notify.shoot (row=7, column=0)
        _, state = self.waitForPhase (daemons, ["shoot"])
      self.assertEqual (state["current"]["state"]["whoseturn"], 0)

      # File a dispute with locked wallet.  That should just silently fail.
      self.mainLogger.info ("Trying dispute that fails...")
      self.lockFunds ()
      self.assertEqual (bar.rpc.filedispute (), "")
      self.expectPendingMoves ("bar", [])
      self.assertEqual (bar.getCurrentState ()["pending"], {})
      self.generate (1)
      state = bar.getCurrentState ()
      assert "dispute" not in state
      self.unlockFunds ()

      # Let bar file a dispute against foo.
      self.mainLogger.info ("Filing a dispute whose resolution fails...")
      bar.rpc.filedispute ()
      self.generate (1)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": False,
        "height": self.env.getChainTip ()[1],
      })

      # Send a new move, but lock the wallet so that the resolution
      # transaction will not succeed.
      self.lockFunds ()
      with self.waitForTurnIncrease (daemons, 2):
        foo.rpc._notify.shoot (row=7, column=0)

      # Unlock the wallet.  Then the resolution transaction should be
      # retried when another block comes in.
      self.mainLogger.info ("Resolving it with unlocked wallet...")
      self.unlockFunds ()
      self.generate (1)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": True,
        "height": self.env.getChainTip ()[1] - 3,
      })
      self.expectPendingMoves ("foo", ["r"])
      self.generate (1)
      _, state = self.waitForPhase (daemons, ["shoot"])
      assert "dispute" not in state

      # Let foo lose the game.  We lock the wallet, so that the loser
      # declaration cannot get sent initially.
      self.mainLogger.info ("Loser declaration fails...")
      self.lockFunds ()
      foo.rpc._notify.revealposition ()
      self.waitForPhase (daemons, ["finished"])
      self.expectPendingMoves ("foo", [])

      # Now unlock the wallet.  Then the next block should re-trigger an update
      # and we should get the move in.
      self.mainLogger.info ("Loser declaration retrial succeeds...")
      self.unlockFunds ()
      self.generate (1)
      state = bar.getCurrentState ()
      self.assertEqual (state["current"]["state"]["parsed"]["winner"], 1)
      self.expectPendingMoves ("foo", ["l"])
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


if __name__ == "__main__":
  TxFailTest ().main ()
