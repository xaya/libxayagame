# Copyright (C) 2018-2021 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Basic framework for integration tests of Xaya games.
"""

from . import game
from . import premine
from . import xaya

import argparse
from contextlib import contextmanager
import copy
import json
import logging
import os.path
import random
import re
import shlex
import shutil
import sys
import time

from jsonrpclib import ProtocolError


XAYAD_BINARY_DEFAULT = "/usr/local/bin/xayad"
DEFAULT_DIR = "/tmp"
DIR_PREFIX = "xayagametest_"


def portGenerator (start):
  """
  Generator that yields a sequence of port numbers for use,
  starting at the given one.
  """

  p = start
  while True:
    yield p
    p += 1


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
    parser.add_argument ("--run_game_with", default="",
                         help="run game daemon with this helper binary"
                              " (e.g. valgrind)")
    parser.add_argument ("--dir", default=DEFAULT_DIR,
                         help="base directory for test runs")
    parser.add_argument ("--nocleanup", default=False, action="store_true",
                         help="do not clean up logs after success")
    self.addArguments (parser)
    self.args = parser.parse_args ()

    # This can be set to "none", "one socket" (default) and "two sockets".
    # That way, tests can control how the ZMQ notifications for pending moves
    # should be sent, if they need to.
    self.zmqPending = "one socket"

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

    # Potentially split multiple parts of the "run_game_with" argument
    # into individual arguments.  If run_game_with with "", then this
    # produces an empty array.
    self.runGameWith = shlex.split (self.args.run_game_with)

    startPort = random.randint (1024, 30000)
    self.log.info ("Using port range starting at %d, hopefully it is free"
        % (startPort))
    self.ports = portGenerator (startPort)

    class RpcHandles:
      game = None
      # Other fields may be set based on the base-chain environment
      # (see baseChainEnvironment).
    self.rpc = RpcHandles ()

    cleanup = False
    success = False
    with self.runBaseChainEnvironment () as env:
      self.env = env
      self.gamenode = self.createGameNode ()
      self.startGameDaemon ()
      try:
        self.setup ()
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
        self.shutdown ()
        self.stopGameDaemon ()

    if cleanup:
      self.log.info ("Cleaning up base directory in %s" % self.basedir)
      shutil.rmtree (self.basedir, ignore_errors=True)

    logging.shutdown ()

    if not success:
      sys.exit ("Test failed")

  def setup (self):
    """
    This method does nothing, but it can be overridden by subclasses to
    provide custom setup functionality.  That is run after setting up the
    base environment (e.g. Xaya Core RPC) but before the actual test
    logic in each test's run() method.
    """

    pass

  def shutdown (self):
    """
    This method does nothing, but it can be overridden by subclasses.
    It gets called when the test run is done and can be used to clean
    up resources.
    """

    pass

  @contextmanager
  def runBaseChainEnvironment (self):
    """
    Constructs a context manager that runs the environment instance
    that should be used for the base chain that the GSP is then linked to.

    By default, this is just a xaya.Environment instance, but tests
    can override this method to e.g. link to a custom chain using Xaya X.
    """

    zmqPorts = {
      "gameblocks": next (self.ports),
    }
    if self.zmqPending == "none":
      self.log.info ("Disabling ZMQ for pending moves in Xaya Core")
    elif self.zmqPending == "one socket":
      self.log.info ("Pending moves are sent on the same socket as blocks")
      zmqPorts["gamepending"] = zmqPorts["gameblocks"]
    elif self.zmqPending == "two sockets":
      self.log.info ("Pending moves are sent on a different socket as blocks")
      zmqPorts["gamepending"] = next (self.ports)
      assert zmqPorts["gamepending"] != zmqPorts["gameblocks"]
    else:
      raise AssertionError ("Invalid zmqPending: %s" % self.zmqPending)

    env = xaya.Environment (self.basedir, next (self.ports), zmqPorts,
                            self.args.xayad_binary)
    with env.run ():
      self.xayanode = env.node
      self.rpc.xaya = env.node.rpc
      yield env


  def run (self):
    self.mainLogger.warning (
        "Test 'run' method not overridden, this tests nothing")

  def startGameDaemon (self, extraArgs=[], wait=True):
    """
    Starts the game daemon (again).  This can be used to test situations where
    the game daemon is restarted and needs to catch up.
    """

    rpcurl, args = self.env.getGspArguments ()
    extraArgs.extend (args)

    self.gamenode.start (rpcurl, extraArgs, wait=wait)
    self.rpc.game = self.gamenode.rpc

  def stopGameDaemon (self):
    """
    Stops the game daemon.  This can be used for testing situations where
    the game daemon is temporarily not running.
    """

    self.rpc.game = None
    self.gamenode.stop ()

  def recreateGameDaemon (self, gameBinary=None, extraArgs=[]):
    """
    Recreates and resyncs from scratch the game daemon.  This can optionally
    set a different binary and extra args for it as well.
    """

    self.stopGameDaemon ()
    self.gamenode = self.createGameNode (gameBinary)
    self.startGameDaemon (extraArgs=extraArgs)

    self.log.info ("Restarted fresh game daemon with binary %s"
                    % self.gamenode.realBinary)

  def createGameNode (self, gameBinary=None):
    """
    Creates a Game instance with the configuration of this test case
    (optionally overriding the GSP binary with the given one).

    This is used internally and should not be called from tests themselves.
    """

    if gameBinary is None:
      gameBinary = self.args.game_daemon

    gameCmd = list (self.runGameWith)
    gameCmd.append (gameBinary)

    return game.Node (self.basedir, next (self.ports), gameCmd)

  ##############################################################################
  # Utility methods for testing.

  def registerNames (self, names):
    """
    Utility method to register names without any data yet.  This can be used
    by tests to set up some initial player accounts for use in the test.
    """

    for nm in names:
      self.env.register ("p", nm)

  def registerOrUpdateName (self, name, value, *args, **kwargs):
    """
    Tries to update or register the name with the given value, depending
    on whether or not it already exists.

    Extra arguments are forwarded directly to the environment's
    move function.
    """

    pos = name.find ("/")
    assert pos > -1, "'%s' contains no namespace" % name

    ns = name[:pos]
    base = name[pos + 1:]

    if not self.env.nameExists (ns, base):
      self.env.register (ns, base)

    return self.env.move (ns, base, value, *args, **kwargs)

  def sendMove (self, name, move, options={}, burn=0):
    """
    Sends a given move for the name.  This calls name_register or name_update,
    depending on whether the name exists already.  It also builds up the
    full name value from self.gameId and move.
    """

    value = json.dumps ({"g": {self.gameId: move}})

    opt = copy.deepcopy (options)

    if burn > 0:
      if "burn" not in opt:
        opt["burn"] = {}
      key = "g/%s" % self.gameId
      assert key not in opt["burn"]
      opt["burn"][key] = burn

    return self.registerOrUpdateName ("p/" + name, value, opt)

  def adminCommand (self, cmd, options={}):
    """
    Sends an admin command with the given value.  This calls name_register or
    name_update, depending on whether or not the g/ name for the game being
    tested already exists or not.
    """

    value = json.dumps ({"cmd": cmd})
    return self.registerOrUpdateName ("g/" + self.gameId, value, options)

  def getCustomState (self, field, method, *args, **kwargs):
    """
    Calls an RPC method on the game daemon that returns game state.  Makes
    sure to wait until the game state is synced.
    """

    fcn = getattr (self.rpc.game, method)

    bestblk, bestheight = self.env.getChainTip ()

    while True:
      state = fcn (*args, **kwargs)
      self.assertEqual (state["gameid"], self.gameId)

      if state["state"] == "up-to-date" and state["blockhash"] == bestblk:
        self.assertEqual (state["height"], bestheight)

        if field is not None:
          return state[field]
        return

      self.log.warning (("Game state (%s, %s) does not match"
                            +" the best block (%s), waiting")
          % (state["state"], state["blockhash"], bestblk))
      time.sleep (0.1)

  def syncGame (self):
    """
    Waits for the game daemon to sync up to the current Xaya Core
    blockchain state.
    """

    self.getCustomState (None, "getnullstate")

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

  def getPendingState (self):
    """
    Returns the current state of pending moves.  Callers must make sure to
    wait some time before calling here themselves, as there is no way to
    ensure this has synced with sent moves.
    """

    return self.getCustomState ("pending", "getpendingstate")

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

    return self.env.generate (n)

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
      (c, m, *_) = exc.args[0]
      self.assertEqual (c, code)
      msgPattern = re.compile (msgRegExp)
      assert msgPattern.match (m)

  def collectPremine (self, addr=None):
    """
    Collects the premine coins (whose keys are publicly known on regtest)
    and sends them to the given address or a new address from the wallet if
    no address is given.  This can be used in tests to obtain a large balance
    and use it for testing purposes.
    """

    premine.collect (self.rpc.xaya, addr, logger=self.log)
