#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests a full channel game including the channel daemons.  There are no edge
cases tested, though (e.g. reorgs), as that is deferred to other
more specialised integration tests.
"""

from shipstest import ShipsTest


class DisputesTest (ShipsTest):

  def run (self):
    self.generate (110)

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

      # Let bar file a dispute against foo.
      self.mainLogger.info ("Filing and resolving a dispute...")
      bar.rpc._notify.filedispute ()
      self.generate (2)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": False,
        "height": self.rpc.xaya.getblockcount () - 1,
      })

      # Resolve the dispute with a new move.
      foo.rpc._notify.shoot (row=7, column=0)
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

      # Simulate a dispute based on a "lost" broadcast message.  It will be
      # resolved immediately (without user interaction) when the dispute
      # gets on-chain.
      self.mainLogger.info ("Resolving dispute from missed broadcast...")
      self.broadcast.setMuted (True)
      foo.rpc._notify.shoot (row=7, column=1)
      bar.rpc._notify.filedispute ()

      self.broadcast.setMuted (False)
      self.generate (1)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": True,
        "height": self.rpc.xaya.getblockcount (),
      })

      self.expectPendingMoves ("foo", ["r"])
      self.generate (1)
      _, state = self.waitForPhase (daemons, ["shoot"])
      assert "dispute" not in state

      self.mainLogger.info ("Closing channel due to dispute...")
      bar.rpc._notify.filedispute ()
      self.generate (10)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": False,
        "height": self.rpc.xaya.getblockcount () - 9,
      })
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
  DisputesTest ().main ()
