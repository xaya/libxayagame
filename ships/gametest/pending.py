#!/usr/bin/env python3
# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the tracking of pending disputes/resolutions in the GSP.
"""

from shipstest import ShipsTest

import time


class PendingTest (ShipsTest):

  def getPendingState (self):
    """
    Returns the pending state, waiting a bit before to "make sure"
    it is synced up.
    """

    time.sleep (1)
    return super ().getPendingState ()

  def expectPendingChannels (self, expected):
    """
    Expects that the pending state contains data about the given
    channels.  expected is a dictionary mapping channel IDs (as hex)
    to the expected turn counts.
    """

    pending = self.getPendingState ()
    assert "channels" in pending

    actual = {}
    for key, val in pending["channels"].items ():
      actual[key] = val["turncount"]

    self.assertEqual (actual, expected)

  def run (self):
    self.generate (110)

    # Pre-register all names we need later for pending.  This ensures that
    # pending tracking also works on EVM chains.
    for nm in ["foo", "bar", "xyz"]:
      self.env.register ("p", nm)
    self.generate (1)

    # Create a test channel with two participants, checking the
    # pending moves while we do it.
    self.mainLogger.info ("Opening a test channel...")
    addr1 = self.newSigningAddress ()
    addr2 = self.newSigningAddress ()
    self.sendMove ("foo", {"c": {"addr": addr1}})
    # We assert the value only after confirming the move, when we can
    # more easily get the channel ID to expect.
    pendingCreate = self.getPendingState ()["create"]
    self.generate (1)
    [cid] = self.getChannelIds ()
    self.assertEqual (pendingCreate, [
      {"name": "foo", "address": addr1, "id": cid},
    ])
    self.sendMove ("bar", {"j": {"id": cid, "addr": addr2}})
    self.assertEqual (self.getPendingState ()["join"], [
      {"name": "bar", "address": addr2, "id": cid},
    ])
    self.generate (1)
    self.expectChannelState (cid, "first commitment", None)

    # Open a dispute and check the pending state.
    self.mainLogger.info ("Filing a dispute...")
    state = self.getStateProof (cid, """
      turn: 1
      position_hashes: "foo 1"
      seed_hash_0: "bar"
    """)
    self.sendMove ("xyz", {"d": {"id": cid, "state": state}})
    self.expectPendingChannels ({cid: 2})

    # Send a resolution move as well.
    self.mainLogger.info ("Resolving the dispute...")
    state = self.getStateProof (cid, """
      turn: 0
      position_hashes: "foo 1"
      position_hashes: "foo 2"
      seed_hash_0: "bar"
      seed_1: "baz"
    """)
    self.sendMove ("xyz", {"r": {"id": cid, "state": state}})
    self.expectPendingChannels ({cid: 3})

    # Mine the moves, which should clear the mempool again.
    self.mainLogger.info ("Mining the pending transactions...")
    self.generate (1)
    self.assertEqual (self.getPendingState (), {
      "channels": {},
      "create": [],
      "join": [],
      "abort": []
    })


if __name__ == "__main__":
  PendingTest ().main ()
