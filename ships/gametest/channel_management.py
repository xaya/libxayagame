#!/usr/bin/env python3
# Copyright (C) 2019-2021 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from shipstest import ShipsTest

"""
Tests basic management of channels, i.e. creating / joining / aborting them
as well as declaring loss to close a channel in agreement.

Disputes and resolutions (which deal with protos and signatures) are left
for another test.
"""


class ChannelManagementTest (ShipsTest):

  def run (self):
    self.generate (110)
    self.expectGameState ({
      "gamestats": {},
      "channels": {},
    })

    # Create three channels with single participants for now.
    self.mainLogger.info ("Creating two channels...")
    addr1 = self.rpc.xaya.getnewaddress ()
    id1 = self.sendMove ("foo", {"c": {"addr": addr1}})
    addr2 = self.rpc.xaya.getnewaddress ()
    id2 = self.sendMove ("bar", {"c": {"addr": addr2}})
    addr3 = self.rpc.xaya.getnewaddress ()
    id3 = self.sendMove ("baz", {"c": {"addr": addr3}})
    self.generate (1)

    state = self.getGameState ()
    self.assertEqual (state["gamestats"], {})
    channels = state["channels"]
    self.assertEqual (len (channels), 3)

    assert id1 in channels
    ch1 = channels[id1]
    self.assertEqual (ch1["meta"]["participants"], [
      {"name": "foo", "address": addr1}
    ])
    self.assertEqual (ch1["state"]["parsed"]["phase"], "single participant")

    assert id2 in channels
    ch2 = channels[id2]
    self.assertEqual (ch2["meta"]["participants"], [
      {"name": "bar", "address": addr2}
    ])
    self.assertEqual (ch2["state"]["parsed"]["phase"], "single participant")

    assert id3 in channels

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
    self.assertEqual (len (channels), 2)

    assert id1 in channels
    ch1 = channels[id1]
    self.assertEqual (ch1["meta"]["participants"], [
      {"name": "foo", "address": addr1},
      {"name": "baz", "address": addr3},
    ])
    self.assertEqual (ch1["state"]["parsed"]["phase"], "first commitment")

    assert id2 not in channels
    assert id3 in channels

    # Let the third channel time out.
    self.generate (9)
    channels = self.getGameState ()["channels"]
    self.assertEqual (len (channels), 2)
    assert id1 in channels
    assert id3 in channels
    self.generate (1)
    channels = self.getGameState ()["channels"]
    self.assertEqual (len (channels), 1)
    assert id1 in channels
    assert id3 not in channels

    # Declare loss in the channel.
    self.mainLogger.info ("Declaring loss in a game to close the channel...")
    self.sendMove ("foo", {"l": {"id": id1, "r": ch1["meta"]["reinit"]}})
    self.generate (1)

    state = self.getGameState ()
    self.assertEqual (state["channels"], {})
    self.assertEqual (state["gamestats"], {
      "foo": {"lost": 1, "won": 0},
      "baz": {"lost": 0, "won": 1},
    })


if __name__ == "__main__":
  ChannelManagementTest ().main ()
