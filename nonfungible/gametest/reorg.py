#!/usr/bin/env python3

# Copyright (C) 2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests behaviour of the GSP with a reorg.
"""

from nftest import NonFungibleTest


class ReorgTest (NonFungibleTest):

  def run (self):
    self.collectPremine ()
    sendTo = {}
    for _ in range (10):
      sendTo[self.rpc.xaya.getnewaddress ()] = 10
    self.rpc.xaya.sendmany ("", sendTo)
    self.generate (1)

    self.sendMove ("domob", [
      {"m": {"a": "foo", "n": 10}},
    ])
    self.generate (10)
    reorgBlk = self.rpc.xaya.getbestblockhash ()

    # First reality:  Transfer assets and do a new mint.
    self.sendMove ("domob", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 7, "r": "andy"}},
      {"m": {"a": "bar", "n": 100, "d": "data 1"}},
      {"t": {"a": {"m": "domob", "a": "bar"}, "n": 50, "r": "daniel"}},
    ])
    self.generate (50)
    self.expectGameState ([
      {
        "asset": {"m": "domob", "a": "bar"},
        "supply": 100,
        "data": "data 1",
        "balances": {"domob": 50, "daniel": 50},
      },
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 3, "andy": 7},
      },
    ])

    # Build an alternate reality where we transfer differently (such that
    # the original transfer is invalid) and also mint the same asset with
    # lower supply.
    oldState = self.getGameState ()
    self.rpc.xaya.invalidateblock (reorgBlk)

    self.sendMove ("domob", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 6, "r": "daniel"}},
      {"m": {"a": "bar", "n": 10, "d": "data 2"}},
    ])
    self.generate (1)
    self.expectGameState ([
      {
        "asset": {"m": "domob", "a": "bar"},
        "supply": 10,
        "data": "data 2",
        "balances": {"domob": 10},
      },
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 4, "daniel": 6},
      },
    ])

    self.rpc.xaya.reconsiderblock (reorgBlk)
    self.expectGameState (oldState)


if __name__ == "__main__":
  ReorgTest ().main ()
