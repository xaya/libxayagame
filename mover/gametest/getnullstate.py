#!/usr/bin/env python3
# Copyright (C) 2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the getnullstate RPC method.
"""

from mover import MoverTest


class GetNullStateTest (MoverTest):

  def run (self):
    self.generate (42)

    # Ensure the GSP is synced up.
    self.syncGame ()

    self.assertEqual (self.rpc.game.getnullstate (), {
      "gameid": "mv",
      "chain": "regtest",
      "state": "up-to-date",
      "height": 42,
      "blockhash": self.rpc.xaya.getbestblockhash (),
    })


if __name__ == "__main__":
  GetNullStateTest ().main ()
