#!/usr/bin/env python3
# Copyright (C) 2019-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the tracking of pending disputes/resolutions in the GSP.
"""

from shipstest import ShipsTest

import time


class PendingTest (ShipsTest):

  def expectPendingChannels (self, expected):
    """
    Expects that the pending state contains data about the given
    channels.  expected is a dictionary mapping channel IDs (as hex)
    to the expected turn counts.
    """

    time.sleep (0.1)

    pending = self.getPendingState ()
    assert "channels" in pending

    actual = {}
    for key, val in pending["channels"].items ():
      actual[key] = val["turncount"]

    self.assertEqual (actual, expected)

  def run (self):
    self.generate (110)

    # Create a test channel with two participants.
    self.mainLogger.info ("Opening a test channel...")
    addr1 = self.rpc.xaya.getnewaddress ()
    addr2 = self.rpc.xaya.getnewaddress ()
    cid = self.openChannel (["foo", "bar"], [addr1, addr2])
    self.expectChannelState (cid, "first commitment", None)

    # Open a dispute and check the pending state.
    self.mainLogger.info ("Filing a dispute...")
    state = self.getStateProof (cid, """
      turn: 1
      position_hashes: "foo"
      seed_hash_0: "bar"
    """)
    self.sendMove ("xyz", {"d": {"id": cid, "state": state}})
    self.expectPendingChannels ({cid: 2})

    # Send a resolution move as well.
    self.mainLogger.info ("Resolving the dispute...")
    state = self.getStateProof (cid, """
      winner: 1
      turn: 0
    """)
    self.sendMove ("xyz", {"r": {"id": cid, "state": state}})
    self.expectPendingChannels ({cid: 4})

    # Mine the moves, which should clear the mempool again.
    self.mainLogger.info ("Mining the pending transactions...")
    self.generate (1)
    self.expectPendingChannels ({})


if __name__ == "__main__":
  PendingTest ().main ()
