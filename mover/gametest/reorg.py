#!/usr/bin/env python3
# Copyright (C) 2018-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

"""
Tests reorgs with the Mover game.
"""


class ReorgTest (MoverTest):

  def run (self):
    self.generate (101)
    self.expectGameState ({"players": {}})

    self.move ("a", "k", 5)
    self.generate (1)
    self.expectGameState ({"players": {
      "a": {"x": 0, "y": 1, "dir": "up", "steps": 4},
    }})

    # Form a branch which moves "a" and registers "b" as new name.  Make it
    # so long that the transactions depend on block rewards from the fork, so
    # that they won't be remined later.  This also means that we will naturally
    # reorg back to this chain later.
    self.generate (1)
    blk = self.rpc.xaya.getbestblockhash ()
    consolidationTxs = []
    for _ in range (11):
      # We have to consolidate in steps to avoid errors because of a too large
      # transaction being generated.
      consolidationTxs.append (self.consolidateCoins ())
      self.generate (10)
    consolidationTxs.append (self.consolidateCoins ())
    self.move ("a", "h", 1)
    self.move ("b", "l", 1)
    self.generate (10)
    self.expectGameState ({"players": {
      "a": {"x": -1, "y": 5},
      "b": {"x": 1, "y": 0},
    }})

    # Invalidate the branch and create another one with different transactions.
    self.rpc.xaya.invalidateblock (blk)
    assert self.rpc.xaya.getrawmempool () == []
    for txid in consolidationTxs:
      # We need to explicitly abandon transactions that involve the orphaned
      # block rewards.  Otherwise the funds won't be marked as available again
      # in the wallet.  See https://github.com/bitcoin/bitcoin/issues/14148.
      self.rpc.xaya.abandontransaction (txid)
    assert self.rpc.xaya.getbalance () > 0
    self.expectGameState ({"players": {
      "a": {"x": 0, "y": 1, "dir": "up", "steps": 4},
    }})
    self.generate (1)
    self.move ("a", "l", 1)
    self.move ("b", "y", 2)
    self.generate (2)
    self.expectGameState ({"players": {
      "a": {"x": 1, "y": 2},
      "b": {"x": -2, "y": 2},
    }})

    # Reorg back to the longer branch.
    self.rpc.xaya.reconsiderblock (blk)
    self.expectGameState ({"players": {
      "a": {"x": -1, "y": 5},
      "b": {"x": 1, "y": 0},
    }})

  def consolidateCoins (self):
    """
    Send all the available balance as a single output back to the wallet.
    This is used to create "dependency" between transactions, so that some
    transactions cannot be remined after detaching a chain part.
    """

    balance = self.rpc.xaya.getbalance ()
    addr = self.rpc.xaya.getnewaddress ()
    self.log.info ("Sending all %.8f CHI to %s" % (balance, addr))
    return self.rpc.xaya.sendtoaddress (addr, balance, "", "", True)


if __name__ == "__main__":
  ReorgTest ().main ()
