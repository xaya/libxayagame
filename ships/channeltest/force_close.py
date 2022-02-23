#!/usr/bin/env python3
# Copyright (C) 2021-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the situation of force-closing a channel with the filedispute RPC
in case the loser does not do so automatically.
"""

from shipstest import ShipsTest


class ForceCloseTest (ShipsTest):

  def run (self):
    self.generate (150)

    self.mainLogger.info ("Creating test channel...")
    channelId, addr = self.openChannel (["foo", "bar"])

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
        bar.rpc._notify.shoot (row=7, column=0)
        _, state = self.waitForPhase (daemons, ["shoot"])
      self.assertEqual (state["current"]["state"]["whoseturn"], 0)

      # Let foo lose the game, but the loser declaration will not be sent
      # because the wallet is locked.
      self.mainLogger.info ("Game is lost without loser declaration...")
      self.lockFunds ()
      foo.rpc._notify.revealposition ()
      self.waitForPhase (daemons, ["finished"])

      # Now unlock the wallet and "force close" the channel by requesting
      # a dispute (which will actually send a resolution move in this case).
      self.mainLogger.info ("Force closing the channel...")
      self.unlockFunds ("bar")
      txid = bar.rpc.filedispute ()
      self.expectPendingMoves ("foo", [])
      pendingTxids = self.expectPendingMoves ("bar", ["r"])
      self.assertEqual (pendingTxids, [txid])
      self.generate (1)
      state = bar.getCurrentState ()
      self.assertEqual (state["existsonchain"], False)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 0, "lost": 1},
          "bar": {"won": 1, "lost": 0},
        },
      })


if __name__ == "__main__":
  ForceCloseTest ().main ()
