#!/usr/bin/env python
# Copyright (C) 2018 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

"""
Tests the waitforchange RPC method behaviour.
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
  Thread subclass that calls the waitforchange RPC method and just blocks
  until it receives the result.
  """

  def __init__ (self, node):
    super (ForChangeWaiter, self).__init__ ()
    self.node = node
    self.result = None
    self.start ()

  def run (self):
    rpc = self.node.createRpc ()
    self.result = rpc.waitforchange ()

  def shouldBeRunning (self):
    sleepSome ()
    assert self.is_alive ()

  def shouldBeDone (self, expected):
    sleepSome ()
    assert not self.is_alive ()
    self.join ()
    assert self.result == expected


class WaitForChangeTest (MoverTest):

  def run (self):
    self.test_attach ()
    self.test_detach ()
    self.test_stopped ()

    # Since the initial game state on regtest is associated with the genesis
    # block already, we cannot test situations where either waitforchange is
    # signaled because of the initial state or where there is not yet a current
    # best block and null is returned from the RPC.

  def test_attach (self):
    self.log.info ("Testing block attaches...")

    waiter = ForChangeWaiter (self.gamenode)
    waiter.shouldBeRunning ()

    self.generate (1)
    waiter.shouldBeDone (self.rpc.xaya.getbestblockhash ())

  def test_detach (self):
    self.log.info ("Testing block detaches...")

    self.generate (1)
    blk = self.rpc.xaya.getbestblockhash ()

    waiter = ForChangeWaiter (self.gamenode)
    waiter.shouldBeRunning ()

    self.rpc.xaya.invalidateblock (blk)
    waiter.shouldBeDone (self.rpc.xaya.getbestblockhash ())

  def test_stopped (self):
    self.log.info ("Testing stopping the daemon while a waiter is active...")

    waiter = ForChangeWaiter (self.gamenode)
    waiter.shouldBeRunning ()

    self.stopGameDaemon ()
    waiter.shouldBeDone (self.rpc.xaya.getbestblockhash ())
    self.startGameDaemon ()


if __name__ == "__main__":
  WaitForChangeTest ().main ()
