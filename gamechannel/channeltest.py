# Copyright (C) 2019-2021 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Integration tests running channel daemons in addition to Xaya Core
on regtest and a GSP.
"""

from . import rpcbroadcast

from xayagametest.testcase import XayaGameTest

import jsonrpclib
import logging
import os
import os.path
import random
import shutil
import subprocess
import threading
import time


class Daemon ():
  """
  An instance of a game's channel daemon, connected to a regtest
  Xaya Core and the game's GSP.

  This class is used for the Xayaships and game-channel integration tests,
  but other games can use it as well if their channel daemons:

  * Have the --xaya_rpc_url, --gsp_rpc_url, --broadcast_rpc_url,
    --rpc_port, --playername and --channelid flags of ships-channel, and
  * provide the "stop" and "getcurrentstate" RPC methods.
  """

  def __init__ (self, channelId, playerName, basedir, port, binary):
    self.log = logging.getLogger ("gamechannel.channeltest.Daemon")
    self.channelId = channelId
    self.playerName = playerName
    self.datadir = os.path.join (basedir, "channel_%s" % playerName)
    self.port = port
    self.binary = binary

    self.log.info ("Creating fresh data directory for the channel daemon in %s"
                    % self.datadir)
    shutil.rmtree (self.datadir, ignore_errors=True)
    os.mkdir (self.datadir)

    self.proc = None

  def start (self, xayarpc, gsprpc, bcrpc, extraArgs=[]):
    if self.proc is not None:
      self.log.error ("Channel process is already running, not starting again")
      return

    self.log.info ("Starting channel daemon for %s" % self.playerName)
    args = [self.binary]
    args.append ("--xaya_rpc_url=%s" % xayarpc)
    args.append ("--gsp_rpc_url=%s" % gsprpc)
    args.append ("--broadcast_rpc_url=%s" % bcrpc)
    args.append ("--rpc_port=%d" % self.port)
    args.append ("--channelid=%s" % self.channelId)
    args.append ("--playername=%s" % self.playerName)
    args.extend (extraArgs)
    envVars = dict (os.environ)
    envVars["GLOG_log_dir"] = self.datadir
    self.proc = subprocess.Popen (args, env=envVars)

    self.rpc = self.createRpc ()
    self.xayaRpc = jsonrpclib.ServerProxy (xayarpc)

    self.log.info ("Waiting for the JSON-RPC server to be up...")
    while True:
      try:
        data = self.rpc.getcurrentstate ()
        self.log.info ("Channel daemon is up for %s" % self.playerName)
        break
      except:
        time.sleep (0.1)

  def stop (self):
    if self.proc is None:
      self.log.error ("No channel process is running cannot stop it")
      return

    self.log.info ("Stopping channel process for %s" % self.playerName)
    self.rpc._notify.stop ()

    self.log.info ("Waiting for channelprocess to stop...")
    self.proc.wait ()
    self.proc = None

  def createRpc (self):
    """
    Returns a freshly created JSON-RPC connection for this daemon.  This can
    be used if multiple threads need to send RPCs in parallel.
    """

    return jsonrpclib.ServerProxy ("http://localhost:%d" % self.port)

  def getCurrentState (self):
    """
    Queries for the current state of the tracked channel.  This also queries
    Xaya Core for the best block hash, and waits until the block known to
    the channel daemon matches it.  That way we can ensure that updates are
    propagated before doing any tests.
    """

    assert self.proc is not None

    bestblk = self.xayaRpc.getbestblockhash ()
    bestheight = self.xayaRpc.getblockcount ()

    while True:
      state = self.rpc.getcurrentstate ()
      if state["blockhash"] == bestblk:
        assert state["height"] == bestheight
        return state

      self.log.warning (("Channel daemon for %s has best block %s,"
                            + " waiting to catch up to %s")
          % (self.playerName, state["blockhash"], bestblk))
      time.sleep (0.01)


class DaemonContext ():
  """
  A context manager that runs a channel daemon and shuts it down upon exit.
  """

  def __init__ (self, daemon, *args, **kwargs):
    self.daemon = daemon
    self.args = args
    self.kwargs = kwargs

  def __enter__ (self):
    self.daemon.start (*self.args, **self.kwargs)
    return self.daemon

  def __exit__ (self, excType, excValue, traceback):
    self.daemon.stop ()


class TestCase (XayaGameTest):
  """
  Integration test case for channel games, which may include testing
  for channel daemons in addition to the GSP.
  """

  def __init__ (self, gameId, gspBinary, channelBinary):
    self.channelBinary = channelBinary
    super (TestCase, self).__init__ (gameId, gspBinary)

  def addArguments (self, parser):
    parser.add_argument ("--channel_daemon", default=self.channelBinary,
                         help="channel daemon binary")

  def setup (self):
    super (TestCase, self).setup ()

    bcPort = next (self.ports)
    self.broadcast = rpcbroadcast.Server ("localhost", bcPort)
    self.bcurl = "http://localhost:%d" % bcPort
    def serveBroadcast ():
      self.broadcast.serve ()
    self.broadcastThread = threading.Thread (target=serveBroadcast)
    self.broadcastThread.start ()

  def shutdown (self):
    self.broadcast.shutdown ()
    self.broadcastThread.join ()
    super (TestCase, self).shutdown ()

  def runChannelDaemon (self, channelId, playerName):
    """
    Starts a new channel daemon for the given ID and player name.
    This returns a context manager instance, which returns the
    underlying Daemon instance when entered.
    """

    daemon = Daemon (channelId, playerName, self.basedir, next (self.ports),
                     self.args.channel_daemon)

    return DaemonContext (daemon, self.xayanode.rpcurl, self.gamenode.rpcurl,
                          self.bcurl)

  def newSigningAddress (self):
    """
    Returns a new address from the local wallet that can be used as signing
    address in a channel.
    """

    return self.rpc.xaya.getnewaddress ("", "legacy")

  def getSyncedChannelState (self, daemons):
    """
    Queries all channel daemons in the passed-in array for their current
    state and returns it when they match up.  This can be used to wait
    with a test until all channel daemons have caught up on some latest
    moves done off-chain.

    Note that the returned state is from one of the daemons, and may contain
    daemon-specific fields (like version).  It is not specified what the
    values of those fields are.
    """

    ok = False
    while not ok:
      ok = True

      state = None
      for c in daemons:
        cur = c.getCurrentState ()

        # If the channel does not exist on chain, there is no point to sync.
        # Each channel daemon is synced to the latest on-chain state anyway
        # already by getCurrentState, and in that situation, there is no
        # additional state to be returned by any of the daemons that could
        # be different.
        if not cur["existsonchain"]:
          return cur

        if state is None:
          state = cur
          continue

        oldTc = state["current"]["state"]["turncount"]
        newTc = cur["current"]["state"]["turncount"]
        if oldTc != newTc:
          ok = False
          break

      self.log.warning ("Channel states differ, waiting...")
      time.sleep (0.01)

    return state
