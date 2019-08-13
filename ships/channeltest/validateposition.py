#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the validateposition RPC method of ships-channel.
"""

from shipstest import ShipsTest


class ValidatePositionTest (ShipsTest):

  def run (self):
    # We need no actual channel, but some fake ID we can pass to ships-channel
    # in order to start the process.
    channelId = "ab" * 32

    self.mainLogger.info ("Starting channel daemons...")
    with self.runChannelDaemon (channelId, "foo") as ch:
      self.mainLogger.info ("Testing validateposition...")
      self.assertEqual (False, ch.rpc.validateposition ("invalid string"))
      self.assertEqual (False, ch.rpc.validateposition ("""
        xx......
        xx......
        ........
        xxxx..xx
        ........
        xxx.xxx.
        ........
        xx.xx.xx
      """))
      self.assertEqual (True, ch.rpc.validateposition ("""
        ........
        ........
        ........
        xxxx..xx
        ........
        xxx.xxx.
        ........
        xx.xx.xx
      """))


if __name__ == "__main__":
  ValidatePositionTest ().main ()
