#!/usr/bin/env python3

# Copyright (C) 2022-2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests the automatic WAL truncation for the SQLite database.
"""

from nftest import NonFungibleTest

import threading
import time

# Regexp for the log when truncation happens.
TRUNCATION_SUCCESS = "Checkpointed and truncated WAL file"


class ReaderThread (threading.Thread):
  """
  A thread that spams read RPCs to the GSP under test.
  """

  def __init__ (self, test):
    super ().__init__ ()

    self.test = test
    self.gsp = test.gamenode.createRpc ()

    self.lock = threading.Lock ()
    self.calls = 0
    self.error = False
    self.shouldStop = False
    self.start ()

  def stop (self):
    with self.lock:
      self.shouldStop = True
    self.join ()

  def run (self):
    while True:
      try:
        self.test.assertEqual (
          self.gsp.getassetdetails ({"m": "domob", "a": "foo"})["data"], {
          "asset": {"m": "domob", "a": "foo"},
          "supply": 10,
          "data": None,
          "balances": {"domob": 10},
        })
      except:
        self.error = True
        break

      # We increment calls after actually running the sucessful call,
      # to ensure a failed or blocked call doesn't count as a successful
      # one made later when we check that some were made during
      # block mining.
      with self.lock:
        if self.shouldStop:
          break
        self.calls += 1


class SQLiteWalTest (NonFungibleTest):

  def run (self):
    self.sendMove ("domob", [{"m": {"a": "foo", "n": 10}}])
    self.generate (1)

    self.log.info ("Turning on periodic WAL truncation...")
    self.stopGameDaemon ()
    assert not self.gamenode.logMatches (TRUNCATION_SUCCESS)
    self.startGameDaemon (extraArgs=["--xaya_sqlite_wal_truncate_ms=1"])

    # We spam RPC requests (that create read snapshots and interfere with
    # WAL truncation) to stress-test the system, and also mine some blocks
    # which will trigger snapshots.
    self.log.info ("Starting reader threads...")
    readers = [ReaderThread (self) for _ in range (10)]

    def getTotalCalls ():
      res = 0
      for r in readers:
        with r.lock:
          res += r.calls
      return res

    self.log.info ("Mining blocks...")
    before = getTotalCalls ()
    for _ in range (10):
      time.sleep (0.01)
      self.generate (1)
    after = getTotalCalls ()
    # Make sure that there were successful calls while mining blocks
    # (and not, for instance, all readers were blocked and just do some
    # calls later while shutting down).
    totalCalls = after - before
    self.mainLogger.info ("Processed %d read calls while mining" % totalCalls)

    self.log.info ("Stopping reader threads...")
    for r in readers:
      r.stop ()

    # Apply final checks for errors once the threads are cleaned up.
    for r in readers:
      if r.error:
        raise RuntimeError ("reader thread failed")
    assert totalCalls > 0

    self.stopGameDaemon ()
    assert self.gamenode.logMatches (TRUNCATION_SUCCESS)


if __name__ == "__main__":
  SQLiteWalTest ().main ()
