#!/usr/bin/env python3
# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests a channel daemon's waitforchange RPC interface.
"""

from shipstest import ShipsTest

from contextlib import contextmanager
import logging
import threading
import time


class WaitForChangeUpdater (threading.Thread):
  """
  Class that runs a waitforchange loop, keeping track of the current state
  that way.  We can then check that this keeps up to date correctly
  in various situations.  If it does, we know that also real frontends
  or the like using such a loop will be fine.

  This is also a context manager that starts and stops the thread.
  """

  def __init__ (self, daemon):
    super (WaitForChangeUpdater, self).__init__ ()
    self.daemon = daemon
    self.rpc = self.daemon.createRpc ()
    self.lock = threading.Lock ()
    self.shouldStop = False
    self.log = logging.getLogger ("WaitForChangeUpdater")

  def run (self):
    with self.lock:
      self.numCalls = 0
      self.state = self.rpc.getcurrentstate ()
      knownVersion = self.state["version"]

    while True:
      upd = self.rpc.waitforchange (knownVersion)
      with self.lock:
        # Only update the call counter if this is a real update, and not
        # just timed out randomly without any change.
        if upd["version"] != knownVersion:
          self.numCalls += 1
        self.state = upd
        knownVersion = self.state["version"]
        if self.shouldStop:
          return

  def stop (self):
    with self.lock:
      self.shouldStop = True
    self.join ()

  def sync (self):
    """
    Queries the daemon explicitly for the current state and waits for
    the updater thread to also reach that state.
    """

    expected = self.daemon.getCurrentState ()

    while True:
      with self.lock:
        assert self.state["version"] <= expected["version"]
        if self.state["version"] == expected["version"]:
          assert self.state == expected
          return

      self.log.warning ("State not yet up-to-date, waiting...")
      time.sleep (0.01)

  def getNumCalls (self):
    """
    Returns the number of times that waitforchange has been called by
    the updater thread.  This can be used to ensure it is not being
    called repeatedly when there should not be any changes (i.e. we do
    not have a busy wait loop).
    """

    with self.lock:
      return self.numCalls

  def __enter__ (self):
    self.start ()
    return self

  def __exit__ (self, excType, excValue, traceback): 
    self.stop ()


class WaitForChangeTest (ShipsTest):

  @contextmanager
  def expectNoUpdates (self, waiter):
    """
    Runs a context during which we expect no updates on the waiter.
    """

    before = waiter.getNumCalls ()
    yield
    after = waiter.getNumCalls ()
    self.assertEqual (after, before)

  def run (self):
    self.generate (110)

    # Create a test channel with two participants.
    self.mainLogger.info ("Creating test channel...")
    channelId, addr = self.openChannel (["foo", "bar"])

    # Start up the two channel daemons.
    self.mainLogger.info ("Starting channel daemons...")
    with self.runChannelDaemon (channelId, "foo", addr[0]) as foo, \
         self.runChannelDaemon (channelId, "bar", addr[1]) as bar, \
         WaitForChangeUpdater (foo) as waiter:

      daemons = [foo, bar]
      waiter.sync ()

      self.mainLogger.info ("Running initialisation sequence...")
      pos = """
        ........
        ........
        ........
        xxxx..xx
        ........
        xxx.xxx.
        ........
        xx.xx.xx
      """
      foo.rpc._notify.setposition (pos)
      bar.rpc._notify.setposition (pos)
      _, state = self.waitForPhase (daemons, ["shoot"])
      waiter.sync ()

      self.mainLogger.info ("No calls if no updates...")
      with self.expectNoUpdates (waiter):
        time.sleep (1)

      # Make sure it is foo's turn.  If not, miss a shot with bar.
      if state["current"]["state"]["whoseturn"] == 1:
        with self.waitForTurnIncrease (daemons, 2):
          bar.rpc._notify.shoot (row=0, column=0)
        _, state = self.waitForPhase (daemons, ["shoot"])
      self.assertEqual (state["current"]["state"]["whoseturn"], 0)

      # Off-chain moves should trigger updates.
      self.mainLogger.info ("Off-chain moves...")
      with self.waitForTurnIncrease (daemons, 2):
        foo.rpc._notify.shoot (row=7, column=0)
      waiter.sync ()

      # Disputes should trigger updates when they get into a block
      # (because blocks trigger updates in general).
      self.mainLogger.info ("On-chain updates...")
      with self.expectNoUpdates (waiter):
        bar.rpc.filedispute ()
        time.sleep (1)
      self.generate (1)
      waiter.sync ()

      # Finally, if we end the game, that should also be reflected in updates.
      self.mainLogger.info ("Ending the game...")
      self.generate (10)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 0, "lost": 1},
          "bar": {"won": 1, "lost": 0},
        },
      })
      state = self.getSyncedChannelState (daemons)
      self.assertEqual (state["existsonchain"], False)
      waiter.sync ()


if __name__ == "__main__":
  WaitForChangeTest ().main ()
