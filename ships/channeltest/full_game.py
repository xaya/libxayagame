#!/usr/bin/env python3
# Copyright (C) 2019-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests a full channel game including the channel daemons.  There are no edge
cases tested, though (e.g. reorgs), as that is deferred to other
more specialised integration tests.
"""

from shipstest import ShipsTest


class FullGameTest (ShipsTest):

  def run (self):
    self.generate (110)

    # Create a test channel with two participants (but with the join
    # not yet confirmed on chain).
    self.mainLogger.info ("Creating test channel...")
    channelId = self.sendMove ("foo", {"c": {
      "addr": self.newSigningAddress (),
    }})
    self.generate (1)
    self.sendMove ("bar", {"j": {
      "id": channelId,
      "addr": self.newSigningAddress (),
    }})

    # Start up the two channel daemons.
    self.mainLogger.info ("Starting channel daemons...")
    with self.runChannelDaemon (channelId, "foo") as foo, \
         self.runChannelDaemon (channelId, "bar") as bar:

      daemons = [foo, bar]
      state = self.getSyncedChannelState (daemons)
      self.assertEqual (state["current"]["state"]["parsed"]["phase"],
                        "single participant")

      # Set foo's position before the channel is created on-chain.  This means
      # that after channel creation, the first commitment will already be
      # done immediately.  The second commitment will be delayed until bar also
      # sets their position.
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

      self.mainLogger.info ("Running initialisation sequence...")
      self.generate (1)
      self.waitForPhase (daemons, ["second commitment"])

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
      self.waitForPhase (daemons, ["shoot"])

      # We play the game as in "FullGameTests/WithShots":  Both players
      # try coordinates in increasing order when it is their turn, which
      # means that bar wins.
      self.mainLogger.info ("Playing the channel...")
      nextCoord = [0, 0]
      while True:
        phase, state = self.waitForPhase (daemons, ["finished", "shoot"])
        if phase == "finished":
          break

        turn = state["current"]["state"]["whoseturn"]
        assert turn in [0, 1]

        target = nextCoord[turn]
        nextCoord[turn] += 1

        row = target // 8
        column = target % 8
        self.log.info ("Player %d shoots at (%d, %d)..." % (turn, row, column))
        daemons[turn].rpc._notify.shoot (row=row, column=column)

      # Verify the result.
      self.mainLogger.info ("Verifying result of channel game...")
      state = self.getSyncedChannelState (daemons)["current"]["state"]["parsed"]
      self.assertEqual (state["phase"], "finished")
      self.assertEqual (state["winner"], 1)

      self.expectPendingMoves ("foo", ["l"])
      self.generate (1)
      self.expectGameState ({
        "channels": {},
        "gamestats":
          {
            "foo": {"won": 0, "lost": 1},
            "bar": {"won": 1, "lost": 0},
          },
      })
      state = self.getSyncedChannelState (daemons)
      self.assertEqual (state["existsonchain"], False)


if __name__ == "__main__":
  FullGameTest ().main ()
