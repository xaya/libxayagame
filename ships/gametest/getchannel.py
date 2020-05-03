#!/usr/bin/env python3
# Copyright (C) 2019-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from shipstest import ShipsTest

"""
Tests the getchannel RPC method of the (generic) channel GSP RPC server.
"""


class GetChannelTest (ShipsTest):

  def run (self):
    self.generate (110)
    self.expectGameState ({
      "gamestats": {},
      "channels": {},
    })

    # Create a test channel and join it.
    self.mainLogger.info ("Creating test channel...")
    addr1 = self.rpc.xaya.getnewaddress ()
    channelId = self.sendMove ("foo", {"c": {"addr": addr1}})
    self.generate (1)
    addr2 = self.rpc.xaya.getnewaddress ()
    self.sendMove ("bar", {"j": {"id": channelId, "addr": addr2}})
    self.generate (1)

    # Verify the channel using getcurrentstate.
    self.mainLogger.info ("Verifying channel data...")
    state = self.getGameState ()
    self.assertEqual (state["gamestats"], {})
    channels = state["channels"]
    self.assertEqual (len (channels), 1)

    assert channelId in channels
    ch = channels[channelId]
    self.assertEqual (ch["meta"]["participants"], [
      {"name": "foo", "address": addr1},
      {"name": "bar", "address": addr2},
    ])
    self.assertEqual (ch["state"]["parsed"]["phase"], "first commitment")

    # Verify what we get with getchannel.
    self.mainLogger.info ("getchannel for our test channel...")
    data = self.getCustomState ("channel", "getchannel", channelId)
    self.assertEqual (data, ch)

    # Error cases for getchannel.
    self.mainLogger.info ("getchannel for non-existant channel...")
    nonExistantId = "aa" * 32
    data = self.getCustomState ("channel", "getchannel", "aa" * 32)
    self.assertEqual (data, None)

    self.mainLogger.info ("getchannel with invalid hex string...")
    self.expectError (-32602, ".*not a valid uint256",
                      self.getCustomState,
                      "channel", "getchannel", "invalid id")


if __name__ == "__main__":
  GetChannelTest ().main ()
