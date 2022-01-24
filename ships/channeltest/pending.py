#!/usr/bin/env python3
# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the handling of pending moves (disputes and resolutions).
"""

from shipstest import ShipsTest


class PendingTest (ShipsTest):

  def run (self):
    self.generate (110)

    # Create a test channel with two participants.
    self.mainLogger.info ("Creating test channel...")
    channelId, addr = self.openChannel (["foo", "bar"])

    # Start up the two channel daemons.
    self.mainLogger.info ("Starting channel daemons...")
    with self.runChannelDaemon (channelId, "foo", addr[0]) as foo, \
         self.runChannelDaemon (channelId, "bar", addr[1]) as bar:

      daemons = [foo, bar]

      self.mainLogger.info ("Running initialisation sequence...")
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
      _, state = self.waitForPhase (daemons, ["shoot"])

      # Make sure it is foo's turn.  If not, miss a shot with bar.
      if state["current"]["state"]["whoseturn"] == 1:
        bar.rpc._notify.shoot (row=7, column=0)
        _, state = self.waitForPhase (daemons, ["shoot"])
      self.assertEqual (state["current"]["state"]["whoseturn"], 0)

      # We want to verify what happens if a dispute does not get mined
      # immediately and is still pending after a new block.  For this,
      # we first mine a block, detach it, then send the move, and then
      # reattach that block.
      self.mainLogger.info ("Testing pending disputes...")
      self.generate (1)
      blk = self.rpc.xaya.getbestblockhash ()
      self.rpc.xaya.invalidateblock (blk)
      txid = bar.rpc.filedispute ()
      self.assertEqual (self.expectPendingMoves ("bar", ["d"]), [txid])
      self.rpc.xaya.reconsiderblock (blk)
      self.assertEqual (bar.rpc.filedispute (), "")
      self.assertEqual (self.expectPendingMoves ("bar", ["d"]), [txid])
      self.assertEqual (bar.getCurrentState ()["pending"], {
        "dispute": txid,
      })
      self.generate (1)
      self.expectPendingMoves ("bar", [])
      self.assertEqual (bar.getCurrentState ()["pending"], {})

      # Now verify what happens in the same situation with a resolution.
      self.mainLogger.info ("Testing pending resolution...")
      self.generate (1)
      blk = self.rpc.xaya.getbestblockhash ()
      self.rpc.xaya.invalidateblock (blk)
      foo.rpc._notify.shoot (row=7, column=0)
      txids = self.expectPendingMoves ("foo", ["r"])
      self.rpc.xaya.reconsiderblock (blk)
      self.assertEqual (self.expectPendingMoves ("foo", ["r"]), txids)
      self.assertEqual (foo.getCurrentState ()["pending"], {
        "resolution": txids[0],
      })
      self.generate (1)
      self.expectPendingMoves ("foo", [])
      self.assertEqual (foo.getCurrentState ()["pending"], {})


if __name__ == "__main__":
  PendingTest ().main ()
