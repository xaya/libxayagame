#!/usr/bin/env python3
# Copyright (C) 2018-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

"""
Tests the waitforchange and waitforpendingchange RPC methods.
"""

import threading
import time


def sleepSome ():
  """
  Sleeps a small amount of time to make it probable that some work in another
  thread has been done.
  """

  time.sleep (0.1)


class ForChangeWaiter (threading.Thread):
  """
  Thread subclass that calls the waitfor(pending)change RPC method and just
  blocks until it receives the result.
  """

  def __init__ (self, test, method, getOldVersion):
    super (ForChangeWaiter, self).__init__ ()
    self.test = test
    self.method = method
    self.oldVersion = getOldVersion (self.createGameRpc ())
    self.result = None
    self.exc = None
    self.start ()
    sleepSome ()

  def createGameRpc (self):
    return self.test.gamenode.createRpc ()

  def run (self):
    fcn = getattr (self.createGameRpc (), self.method)
    try:
      self.result = fcn (self.oldVersion)
    except Exception as exc:
      self.test.log.exception (exc)
      self.exc = exc

  def shouldBeRunning (self):
    sleepSome ()
    assert self.is_alive ()

  def shouldBeDone (self, expected=None):
    sleepSome ()
    assert not self.is_alive ()
    self.join ()
    if self.exc is not None:
      raise self.exc
    if expected is not None:
      self.test.assertEqual (self.result, expected)


class WaitForChangeTest (MoverTest):

  def run (self):
    self.generate (101)

    self.test_attach ()
    self.test_detach ()
    self.test_move ()
    self.test_pending_disabled ()
    self.test_stopped ()

    # Since the initial game state on regtest is associated with the genesis
    # block already, we cannot test situations where either waitforchange is
    # signaled because of the initial state or where there is not yet a current
    # best block and null is returned from the RPC.

  def getBlockChangeWaiter (self):
    """
    Returns a ForChangeWaiter instance calling waitforchange.
    """

    def getOldVersion (rpc):
      state = rpc.getcurrentstate ()
      if "blockhash" in state:
        return state["blockhash"]
      return ""

    return ForChangeWaiter (self, "waitforchange", getOldVersion)

  def getPendingChangeWaiter (self):
    """
    Returns a ForChangeWaiter instance calling waitforpendingchange.
    """

    def getOldVersion (rpc):
      state = rpc.getpendingstate ()
      if "version" in state:
        return state["version"]
      return 0

    return ForChangeWaiter (self, "waitforpendingchange", getOldVersion)

  def test_attach (self):
    self.mainLogger.info ("Block attaches...")

    blocks = self.getBlockChangeWaiter ()
    pending = self.getPendingChangeWaiter ()
    blocks.shouldBeRunning ()
    pending.shouldBeRunning ()

    self.generate (1)
    blocks.shouldBeDone (self.rpc.xaya.getbestblockhash ())
    pending.shouldBeDone (self.rpc.game.getpendingstate ())

  def test_detach (self):
    self.mainLogger.info ("Block detaches...")

    self.generate (1)
    blk = self.rpc.xaya.getbestblockhash ()

    blocks = self.getBlockChangeWaiter ()
    pending = self.getPendingChangeWaiter ()
    blocks.shouldBeRunning ()
    pending.shouldBeRunning ()

    self.rpc.xaya.invalidateblock (blk)
    blocks.shouldBeDone (self.rpc.xaya.getbestblockhash ())
    pending.shouldBeDone (self.rpc.game.getpendingstate ())

  def test_move (self):
    self.mainLogger.info ("Move sent...")

    pending = self.getPendingChangeWaiter ()
    pending.shouldBeRunning ()

    self.move ("a", "k", 5)
    pending.shouldBeDone (self.rpc.game.getpendingstate ())

  def test_pending_disabled (self):
    self.mainLogger.info ("Tracking of pending moves disabled...")

    self.stopGameDaemon ()
    self.startGameDaemon (extraArgs=["--nopending_moves"])

    self.expectError (-32603, ".*pending moves are not tracked.*",
                      self.rpc.game.waitforpendingchange, 0)

    self.stopGameDaemon ()
    self.startGameDaemon ()

  def test_stopped (self):
    self.mainLogger.info ("Stopping the daemon while a waiter is active...")

    blocks = self.getBlockChangeWaiter ()
    pending = self.getPendingChangeWaiter ()
    blocks.shouldBeRunning ()
    pending.shouldBeRunning ()

    self.stopGameDaemon ()
    blocks.shouldBeDone (self.rpc.xaya.getbestblockhash ())
    pending.shouldBeDone ()
    self.startGameDaemon ()


if __name__ == "__main__":
  WaitForChangeTest ().main ()
