# Copyright (C) 2018-2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Basic framework for integration tests of Xaya games.
"""

import game
import xaya

import argparse
import json
import logging
import os.path
import random
import re
import shutil
import sys
import time

from jsonrpclib import ProtocolError


XAYAD_BINARY_DEFAULT = "/usr/local/bin/xayad"
DEFAULT_DIR = "/tmp"
DIR_PREFIX = "xayagametest_"


class XayaGameTest (object):
  """
  Base class for integration test cases of Xaya games.  This manages the
  Xaya Core daemon, the game daemon, basic logging and the data directory.

  The actual test should override the "run" method with its test logic.  It
  can control the Xaya Core daemon through rpc.xaya and the game daemon through
  rpc.game.
  """

  ##############################################################################
  # Main functionality, handling the setup of daemons and all that.

  def __init__ (self, gameId, gameBinaryDefault):
    self.gameId = gameId

    desc = "Runs an integration test for the Xaya game g/%s." % gameId
    parser = argparse.ArgumentParser (description=desc)
    parser.add_argument ("--xayad_binary", default=XAYAD_BINARY_DEFAULT,
                         help="xayad binary to use in the test")
    parser.add_argument ("--game_daemon", default=gameBinaryDefault,
                         help="game daemon binary to use in the test")
    parser.add_argument ("--dir", default=DEFAULT_DIR,
                         help="base directory for test runs")
    parser.add_argument ("--nocleanup", default=False, action="store_true",
                         help="do not clean up logs after success")
    self.addArguments (parser)
    self.args = parser.parse_args ()

  def addArguments (self, parser):
    """
    This function is called to add additional arguments (test specific)
    for the argument parser.  By default, none are added, but subclasses
    can override it as needed.
    """

    pass

  def main (self):
    """
    Executes the testcase, including setup and cleanup.
    """

    randomSuffix = "%08x" % random.getrandbits (32)
    self.basedir = os.path.join (self.args.dir, DIR_PREFIX + randomSuffix)
    shutil.rmtree (self.basedir, ignore_errors=True)
    os.mkdir (self.basedir)

    logfile = os.path.join (self.basedir, "xayagametest.log")
    logHandler = logging.FileHandler (logfile)
    logFmt = "%(asctime)s %(name)s (%(levelname)s): %(message)s"
    logHandler.setFormatter (logging.Formatter (logFmt))

    rootLogger = logging.getLogger ()
    rootLogger.setLevel (logging.INFO)
    rootLogger.addHandler (logHandler)

    self.log = logging.getLogger ("xayagametest.testcase")

    mainHandler = logging.StreamHandler (sys.stderr)
    mainHandler.setFormatter (logging.Formatter ("%(message)s"))

    self.mainLogger = logging.getLogger ("main")
    self.mainLogger.addHandler (logHandler)
    self.mainLogger.addHandler (mainHandler)
    self.mainLogger.info ("Base directory for integration test: %s"
                            % self.basedir)

    basePort = random.randint (1024, 30000)
    self.log.info ("Using port range %d..%d, hopefully it is free"
        % (basePort, basePort + 2))

    self.xayanode = xaya.Node (self.basedir, basePort, basePort + 1,
                               self.args.xayad_binary)
    self.gamenode = game.Node (self.basedir, basePort + 2,
                               self.args.game_daemon)

    class RpcHandles:
      xaya = None
      game = None
    self.rpc = RpcHandles ()

    self.startXayaDaemon ()
    cleanup = False
    success = False
    try:
      self.startGameDaemon ()
      try:
        self.run ()
        self.mainLogger.info ("Test succeeded")
        success = True
        if self.args.nocleanup:
          self.mainLogger.info ("Not cleaning up logs as requested")
        else:
          cleanup = True
      except:
        self.mainLogger.exception ("Test failed")
        self.log.info ("Not cleaning up base directory %s" % self.basedir)
      finally:
        self.stopGameDaemon ()
    finally:
      self.stopXayaDaemon ()
      if cleanup:
        self.log.info ("Cleaning up base directory in %s" % self.basedir)
        shutil.rmtree (self.basedir, ignore_errors=True)
      logging.shutdown ()

    if not success:
      sys.exit ("Test failed")

  def run (self):
    self.mainLogger.warning (
        "Test 'run' method not overridden, this tests nothing")

  def startXayaDaemon (self):
    """
    Starts the Xaya Core daemon.
    """

    self.xayanode.start ()
    self.rpc.xaya = self.xayanode.rpc

  def stopXayaDaemon (self):
    """
    Stops the Xaya Core daemon.
    """

    self.xayanode.stop ()

  def startGameDaemon (self, extraArgs=[]):
    """
    Starts the game daemon (again).  This can be used to test situations where
    the game daemon is restarted and needs to catch up.
    """

    self.gamenode.start (self.xayanode.rpcurl, extraArgs)
    self.rpc.game = self.gamenode.rpc

  def stopGameDaemon (self):
    """
    Stops the game daemon.  This can be used for testing situations where
    the game daemon is temporarily not running.
    """

    self.rpc.game = None
    self.gamenode.stop ()

  ##############################################################################
  # Utility methods for testing.

  def registerNames (self, names):
    """
    Utility method to register names without any data yet.  This can be used
    by tests to set up some initial player accounts for use in the test.
    """

    for nm in names:
      self.rpc.xaya.name_register ("p/" + nm, "{}")

  def registerOrUpdateName (self, name, value, options={}):
    """
    Tries to update or register the name with the given value, depending
    on whether or not it already exists.
    """

    try:
      return self.rpc.xaya.name_update (name, value, options)
    except:
      self.log.info ("name_update for %s failed, trying name_register" % name)
      return self.rpc.xaya.name_register (name, value, options)

  def sendMove (self, name, move, options={}):
    """
    Sends a given move for the name.  This calls name_register or name_update,
    depending on whether the name exists already.  It also builds up the
    full name value from self.gameId and move.
    """

    value = json.dumps ({"g": {self.gameId: move}})
    return self.registerOrUpdateName ("p/" + name, value, options)

  def adminCommand (self, cmd, options={}):
    """
    Sends an admin command with the given value.  This calls name_register or
    name_update, depending on whether or not the g/ name for the game being
    tested already exists or not.
    """

    value = json.dumps ({"cmd": cmd})
    return self.registerOrUpdateName ("g/" + self.gameId, value, options)

  def getCustomState (self, field, method, **kwargs):
    """
    Calls an RPC method on the game daemon that returns game state.  Makes
    sure to wait until the game state is synced.
    """

    fcn = getattr (self.rpc.game, method)

    bestblk = self.rpc.xaya.getbestblockhash ()
    bestheight = self.rpc.xaya.getblockcount ()

    while True:
      state = fcn (**kwargs)
      self.assertEqual (state["gameid"], self.gameId)

      if state["state"] == "up-to-date" and state["blockhash"] == bestblk:
        self.assertEqual (state["height"], bestheight)
        return state[field]

      self.log.warning (("Game state (%s, %s) does not match"
                            +" the best block (%s), waiting")
          % (state["state"], state["blockhash"], bestblk))
      time.sleep (0.1)

  def getGameState (self):
    """
    Returns the current game state.  Makes sure to wait for the game daemon
    to sync up with Xaya's best block first.
    """

    return self.getCustomState ("gamestate", "getcurrentstate")

  def expectGameState (self, expected):
    """
    Expects that the current game state matches the given value.
    """

    actual = self.getGameState ()
    self.assertEqual (actual, expected)

  def assertEqual (self, a, b):
    """
    Asserts that two values are equal, logging them if not.
    """

    if a == b:
      return

    self.log.error ("The value of:\n%s\n\nis not equal to:\n%s" % (a, b))
    raise AssertionError ("%s != %s" % (a, b))

  def generate (self, n):
    """
    Generates n new blocks on the Xaya network.
    """

    addr = self.rpc.xaya.getnewaddress ()
    self.rpc.xaya.generatetoaddress (n, addr)

  def expectError (self, code, msgRegExp, method, *args, **kwargs):
    """
    Calls the method object with the given arguments, and expects that
    an RPC error is raised matching the code and message.
    """

    try:
      method (*args, **kwargs)
      self.log.error ("Expected RPC error with code=%d and message %s"
                        % (code, msgRegExp))
      raise AssertionError ("expected RPC error was not raised")
    except ProtocolError as exc:
      self.log.info ("Caught expected RPC error: %s" % exc)
      (c, m) = exc.args[0]
      self.assertEqual (c, code)
      msgPattern = re.compile (msgRegExp)
      assert msgPattern.match (m)
