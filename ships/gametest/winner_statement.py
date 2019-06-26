#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from shipstest import ShipsTest

"""
Tests closing a channel in agreement (with a signed winner statement).
"""


class WinnerStatementTest (ShipsTest):

  def run (self):
    self.generate (110)

    # Create a test channel with two participants.  Only the signing
    # address of one of them is in our wallet, though, so that the
    # winner statement will only be signed by that one.
    self.mainLogger.info ("Opening a test channel...")
    addr = self.rpc.xaya.getnewaddress ()
    cid = self.openChannel (["foo", "bar"], [addr, "other address"])

    # Construct a winner statement that would need to be signed by bar
    # and make sure this does not do anything to the channel (as it will
    # be signed only by foo).
    self.mainLogger.info ("Trying invalid winner statement...")
    stmt = self.getWinnerStatement (cid, 0)
    self.sendMove ("xyz", {"w": {"id": cid, "stmt": stmt}})
    self.generate (1)

    state = self.getGameState ()
    self.assertEqual (state["gamestats"], {})
    self.assertEqual (len (state["channels"]), 1)
    assert cid in state["channels"]

    # Perform a successful close with a valid statement.
    self.mainLogger.info ("Closing with valid statement...")
    stmt = self.getWinnerStatement (cid, 1)
    self.sendMove ("xyz", {"w": {"id": cid, "stmt": stmt}})
    self.generate (1)

    self.expectGameState ({
      "gamestats":
        {
          "foo": {"won": 0, "lost": 1},
          "bar": {"won": 1, "lost": 0},
        },
      "channels": {},
    })

    # Make sure that a replay attack does not work.
    self.mainLogger.info ("Testing replay attack...")
    newCid = self.openChannel (["foo", "bar"], [addr, "other address"])
    assert newCid != cid
    self.sendMove ("xyz", {"w": {"id": newCid, "stmt": stmt}})
    self.generate (1)

    state = self.getGameState ()
    self.assertEqual (state["gamestats"], {
      "foo": {"won": 0, "lost": 1},
      "bar": {"won": 1, "lost": 0},
    })
    assert newCid in state["channels"]


if __name__ == "__main__":
  WinnerStatementTest ().main ()
