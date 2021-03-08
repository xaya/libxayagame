#!/usr/bin/env python3
# Copyright (C) 2021 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests "force-closing" a channel if the winner is determined but the
loser does not declare loss by themselves.
"""

from shipstest import ShipsTest


class ForceCloseTest (ShipsTest):

  def run (self):
    self.generate (110)

    # Create a test channel with two participants.
    self.mainLogger.info ("Opening a test channel...")
    addr1 = self.rpc.xaya.getnewaddress ()
    addr2 = self.rpc.xaya.getnewaddress ()
    cid = self.openChannel (["foo", "bar"], [addr1, addr2])
    self.expectChannelState (cid, "first commitment", None)

    # Filing a dispute at the end state doesn't work as it is a no-turn
    # situation.
    state = self.getStateProof (cid, """
      winner: 0
    """)
    self.sendMove ("xyz", {"d": {"id": cid, "state": state}})
    self.generate (1)
    self.expectChannelState (cid, "first commitment", None)

    # With a resolution, we can force-close the channel.
    self.mainLogger.info ("Force closing the channel...")
    self.sendMove ("xyz", {"r": {"id": cid, "state": state}})
    self.generate (1)
    self.expectGameState ({
      "gamestats":
        {
          "foo": {"won": 1, "lost": 0},
          "bar": {"won": 0, "lost": 1},
        },
      "channels": {},
    })


if __name__ == "__main__":
  ForceCloseTest ().main ()
