#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from shipstest import ShipsTest

"""
Tests basic management of channels, i.e. creating / joining / aborting them.
The more sophisticated situations (which deal with protos and signatures)
are left for other tests.
"""


class ChannelManagementTest (ShipsTest):

  def run (self):
    self.generate (110)
    self.expectGameState ({
      "gamestats": {},
      "channels": {},
    })

    # Create two channels with single participants for now.
    self.mainLogger.info ("Creating two channels...")
    addr1 = self.rpc.xaya.getnewaddress ()
    id1 = self.sendMove ("foo", {"c": {"addr": addr1}})
    addr2 = self.rpc.xaya.getnewaddress ()
    id2 = self.sendMove ("bar", {"c": {"addr": addr2}})
    self.generate (1)

    state = self.getGameState ()
    self.assertEqual (state["gamestats"], {})
    channels = state["channels"]
    self.assertEqual (len (channels), 2)

    assert id1 in channels
    ch1 = channels[id1]
    self.assertEqual (ch1["meta"]["participants"], [
      {"name": "foo", "address": addr1}
    ])
    self.assertEqual (ch1["state"]["data"]["phase"], "single participant")

    assert id2 in channels
    ch2 = channels[id2]
    self.assertEqual (ch2["meta"]["participants"], [
      {"name": "bar", "address": addr2}
    ])
    self.assertEqual (ch2["state"]["data"]["phase"], "single participant")

    # Perform an invalid join and abort on the channels. This should not affect
    # the state at all.
    self.mainLogger.info ("Trying invalid operations...")
    addr3 = self.rpc.xaya.getnewaddress ()
    self.sendMove ("foo", {"j": {"id": id1, "addr": addr3}})
    self.sendMove ("baz", {"a": {"id": id2}})
    self.generate (1)
    self.expectGameState (state)

    # Join one of the channels and abort the other, this time for real.
    self.mainLogger.info ("Joining and aborting the channels...")
    self.sendMove ("baz", {"j": {"id": id1, "addr": addr3}})
    self.sendMove ("bar", {"a": {"id": id2}})
    self.generate (1)

    state = self.getGameState ()
    self.assertEqual (state["gamestats"], {})
    channels = state["channels"]
    self.assertEqual (len (channels), 1)

    assert id1 in channels
    ch1 = channels[id1]
    self.assertEqual (ch1["meta"]["participants"], [
      {"name": "foo", "address": addr1},
      {"name": "baz", "address": addr3},
    ])
    self.assertEqual (ch1["state"]["data"]["phase"], "first commitment")

    assert id2 not in channels


if __name__ == "__main__":
  ChannelManagementTest ().main ()
