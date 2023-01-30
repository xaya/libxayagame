#!/usr/bin/env python3

# Copyright (C) 2023 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests game-state hashing in the GSP.
"""

from nftest import NonFungibleTest

import time


class StatehashTest (NonFungibleTest):

  def hashState (self):
    return self.getRpc ("hashcurrentstate")

  def run (self):
    # Mint some assets and get to an even block height (so we know for sure
    # which blocks will be hashed and which not) before the reorg point.
    self.sendMove ("domob", [
      {"m": {"a": "foo", "n": 10}},
    ])
    self.generate (10)
    if self.env.getChainTip ()[1] % 2 == 1:
      self.generate (1)
    hash0 = self.hashState ()
    baseBlk, _ = self.env.getChainTip ()
    self.assertEqual (
        hash0,
        "62b033bfc82496a027dde72de4d81e0341de012eaa921194656c119ac88f6761")
    self.generate (1)
    reorgBlk, height = self.env.getChainTip ()
    assert height % 2 == 1

    # This is the chain structure we will build:
    # baseBlk
    #   |-- reorgBlk -- blk0p -- x -- blk1
    #   \--- oddBlk --- blk2 -- y

    # Without automatic hashing turned on, build up a chain and manually
    # hash the state at some points.
    self.assertEqual (self.hashState (), hash0)
    blkOdd, _ = self.env.getChainTip ()
    self.generate (1)
    self.assertEqual (self.hashState (), hash0)
    blk0p, _ = self.env.getChainTip ()
    self.sendMove ("domob", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 7, "r": "andy"}},
    ])
    self.generate (2)
    self.expectGameState ([
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 3, "andy": 7},
      },
    ])
    hash1 = self.hashState ()
    blk1, _ = self.env.getChainTip ()
    assert hash1 != hash0

    # Hashing is not turned on yet, so getstatehash should fail.
    self.expectError (-32601, ".*state hashing is not enabled.*",
                      self.getRpc, "getstatehash", reorgBlk)

    # Turn on hashing of every second block.
    self.log.info ("Turning on hashing...")
    self.stopGameDaemon ()
    self.startGameDaemon (extraArgs=["--statehash_interval=2"])

    # The blocks are still not hashed for now.
    self.assertEqual (self.getRpc ("getstatehash", baseBlk), None)
    self.assertEqual (self.getRpc ("getstatehash", blk0p), None)
    self.assertEqual (self.getRpc ("getstatehash", blk1), None)

    # Reorg and build up the second (shorter chain).  Hashing
    # is now turned on, so along the way all even blocks will be
    # hashed (including on the detach).
    self.rpc.xaya.invalidateblock (reorgBlk)
    self.sendMove ("domob", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 7, "r": "daniel"}},
    ])
    self.generate (1)
    self.expectGameState ([
      {
        "asset": {"m": "domob", "a": "foo"},
        "supply": 10,
        "data": None,
        "balances": {"domob": 3, "daniel": 7},
      },
    ])
    oddBlk, _ = self.env.getChainTip ()
    self.generate (1)
    hash2 = self.hashState ()
    blk2, _ = self.env.getChainTip ()
    assert hash2 not in [hash0, hash1]

    # Check the stored hashes.
    self.assertEqual (self.getRpc ("getstatehash", baseBlk), hash0)
    self.assertEqual (self.getRpc ("getstatehash", blk0p), hash0)
    # blk1 is still not hashed for now, since we only turned on hashing
    # when it was already active, and it has not "become" the tip state
    # since yet.
    self.assertEqual (self.getRpc ("getstatehash", blk1), None)
    self.assertEqual (self.getRpc ("getstatehash", oddBlk), None)
    # Due to async processing, we need to wait and also trigger
    # another run of the processor before it gets done.
    time.sleep (0.1)
    self.generate (1)
    self.syncGame ()
    self.assertEqual (self.getRpc ("getstatehash", blk2), hash2)

    # Reorg back to the original chain.
    self.rpc.xaya.reconsiderblock (reorgBlk)
    self.syncGame ()
    self.assertEqual (self.env.getChainTip ()[0], blk1)

    # Check stored hashes again.  Now all should be there.
    # We restart the GSP to make sure all running processes are done
    # and stored.
    self.stopGameDaemon ()
    self.startGameDaemon (extraArgs=["--statehash_interval=2"])
    self.assertEqual (self.getRpc ("getstatehash", baseBlk), hash0)
    self.assertEqual (self.getRpc ("getstatehash", blk0p), hash0)
    self.assertEqual (self.getRpc ("getstatehash", blk1), hash1)
    self.assertEqual (self.getRpc ("getstatehash", blk2), hash2)


if __name__ == "__main__":
  StatehashTest ().main ()
