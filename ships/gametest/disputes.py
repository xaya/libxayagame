#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from shipstest import ShipsTest

"""
Tests disputes and resolutions in games.
"""


class DisputeTest (ShipsTest):

  def run (self):
    self.generate (110)

    # Create a test channel with two participants.
    self.mainLogger.info ("Opening a test channel...")
    addr1 = self.rpc.xaya.getnewaddress ()
    addr2 = self.rpc.xaya.getnewaddress ()
    cid = self.openChannel (["foo", "bar"], [addr1, addr2])
    self.expectChannelState (cid, "first commitment", None)

    # Open a dispute.
    self.mainLogger.info ("Filing a dispute...")
    state = self.getStateProof (cid, "turn: 0")
    self.sendMove ("xyz", {"d": {"id": cid, "state": state}})
    self.generate (1)
    disputeHeight = self.rpc.xaya.getblockcount ()
    self.expectChannelState (cid, "first commitment", disputeHeight)

    # Resolving it without giving more moves won't work.
    self.mainLogger.info ("Trying to resolve without more moves...")
    self.sendMove ("xyz", {"r": {"id": cid, "state": state}})
    self.generate (1)
    self.expectChannelState (cid, "first commitment", disputeHeight)

    # Successfully resolve now, right at the expiry height.
    self.generate (8)
    state = self.getStateProof (cid, """
      turn: 1
      position_hashes: "foo"
      seed_hash_0: "bar"
    """)
    self.sendMove ("xyz", {"r": {"id": cid, "state": state}})
    self.generate (1)
    self.assertEqual (self.rpc.xaya.getblockcount (), disputeHeight + 10)
    self.expectChannelState (cid, "second commitment", None)

    # File a dispute right after resolution, with the same state.
    self.generate (100)
    self.mainLogger.info ("Disputing same state again...")
    self.sendMove ("xyz", {"d": {"id": cid, "state": state}})
    self.generate (1)
    disputeHeight = self.rpc.xaya.getblockcount ()
    self.expectChannelState (cid, "second commitment", disputeHeight)

    # Let it expire and the first player win.
    self.mainLogger.info ("Letting dispute expire...")
    self.generate (10)
    self.assertEqual (self.rpc.xaya.getblockcount (), disputeHeight + 10)
    self.expectGameState ({
      "gamestats":
        {
          "foo": {"won": 1, "lost": 0},
          "bar": {"won": 0, "lost": 1},
        },
      "channels": {},
    })


if __name__ == "__main__":
  DisputeTest ().main ()
