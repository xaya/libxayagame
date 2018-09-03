# Copyright (C) 2018 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Basic framework for integration tests of Xaya games.
"""

import game
import xaya

import argparse
import logging
import os.path
import shutil
import sys
import time


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

  def __init__ (self, name, gameBinaryDefault):
    desc = "Runs an integration test for the Xaya game %s." % name
    parser = argparse.ArgumentParser (description=desc)
    parser.add_argument ("--xayad_binary", default=XAYAD_BINARY_DEFAULT,
                         help="xayad binary to use in the test")
    parser.add_argument ("--game_daemon", default=gameBinaryDefault,
                         help="game daemon binary to use in the test")
    parser.add_argument ("--dir", default=DEFAULT_DIR,
                         help="base directory for test runs")
    self.args = parser.parse_args ()

  def main (self):
    timefmt = "%Y%m%d_%H%M%S"
    self.basedir = os.path.join (self.args.dir,
                                 DIR_PREFIX + time.strftime (timefmt))
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

    mainLogger = logging.getLogger ("main")
    mainLogger.addHandler (logHandler)
    mainLogger.addHandler (mainHandler)
    mainLogger.info ("Base directory for integration test: %s" % self.basedir)

    self.xayanode = xaya.Node (self.basedir, self.args.xayad_binary)
    self.gamenode = game.Node (self.basedir, self.args.game_daemon)

    class RpcHandles:
      xaya = None
      game = None
    self.rpc = RpcHandles ()

    self.xayanode.start ()
    self.rpc.xaya = self.xayanode.rpc
    cleanup = False
    try:
      self.gamenode.start (self.xayanode.rpcurl)
      self.rpc.game = self.gamenode.rpc
      try:
        self.run ()
        mainLogger.info ("Test succeeded")
        cleanup = True
      except:
        mainLogger.exception ("Test failed")
        self.log.info ("Not cleaning up base directory %s" % self.basedir)
      finally:
        self.gamenode.stop ()
    finally:
      self.xayanode.stop ()
      if cleanup:
        self.log.info ("Cleaning up base directory in %s" % self.basedir)
        shutil.rmtree (self.basedir, ignore_errors=True)
      logging.shutdown ()

  def run (self):
    self.log.warning ("Test 'run' method not overridden, this tests nothing")
