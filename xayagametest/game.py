# Copyright (C) 2018 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Code for running a game daemon as component in an integration test.
"""

import jsonrpclib
import logging
import os
import os.path
import shutil
import subprocess
import time


class Node ():
  """
  An instance of a game daemon that is connected to a regtest Xaya Core node
  and used in an integration test.

  It is implemented for the Mover game daemon included with libxayagame, but
  other game daemons are supported as well if they:

  - Have the --xaya_rpc_url and --game_rpc_port flags that moverd has, and
  - provide at least the "stop" and "getcurrentstate" RPC methods.
  """

  def __init__ (self, basedir, port, binary):
    self.log = logging.getLogger ("xayagametest.gamenode")
    self.datadir = os.path.join (basedir, "gamenode")
    self.port = port
    self.binary = binary

    self.log.info ("Creating fresh data directory for the game node in %s"
                    % self.datadir)
    shutil.rmtree (self.datadir, ignore_errors=True)
    os.mkdir (self.datadir)

    self.proc = None

  def start (self, xayarpc, extraArgs=[]):
    if self.proc is not None:
      self.log.error ("Game process is already running, not starting again")
      return

    self.log.info ("Starting new game process")
    args = [self.binary]
    args.append ("--xaya_rpc_url=%s" % xayarpc)
    args.append ("--game_rpc_port=%d" % self.port)
    args.extend (extraArgs)
    envVars = dict (os.environ)
    envVars["GLOG_log_dir"] = self.datadir
    self.proc = subprocess.Popen (args, env=envVars)

    self.rpc = jsonrpclib.Server ("http://localhost:%d" % self.port)

    self.log.info ("Waiting for the JSON-RPC server to be up...")
    while True:
      try:
        data = self.rpc.getcurrentstate ()
        self.log.info ("Game daemon is up, chain = %s" % data["chain"])
        break
      except:
        time.sleep (1)

  def stop (self):
    if self.proc is None:
      self.log.error ("No game process is running cannot stop it")
      return

    self.log.info ("Stopping game process")
    self.rpc._notify.stop ()

    self.log.info ("Waiting for game process to stop...")
    self.proc.wait ()
    self.proc = None
