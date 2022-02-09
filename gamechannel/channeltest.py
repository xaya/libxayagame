# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Integration tests running channel daemons in addition to Xaya Core
on regtest and a GSP.
"""

from . import rpcbroadcast

from xayagametest.testcase import XayaGameTest

from eth_account import Account, messages

from contextlib import contextmanager
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

  * Have the --playername and --rpc_port flags of ships-channel, and
  * provide the "stop" and "getcurrentstate" RPC methods.
  """

  def __init__ (self, playerName, basedir, port, binary):
    self.log = logging.getLogger ("gamechannel.channeltest.Daemon")
    self.playerName = playerName
    self.datadir = os.path.join (basedir, "channel_%s" % playerName)
    self.port = port
    self.binary = binary

    self.log.info ("Creating fresh data directory for the channel daemon in %s"
                    % self.datadir)
    shutil.rmtree (self.datadir, ignore_errors=True)
    os.mkdir (self.datadir)

    self.proc = None

  def start (self, env, **kwargs):
    if self.proc is not None:
      self.log.error ("Channel process is already running, not starting again")
      return

    self.log.info ("Starting channel daemon for %s" % self.playerName)
    args = [self.binary]
    args.append ("--rpc_port=%d" % self.port)
    args.append ("--playername=%s" % self.playerName)
    args.extend ([
      "--%s=%s" % (k, v)
      for k, v in kwargs.items ()
    ])
    envVars = dict (os.environ)
    envVars["GLOG_log_dir"] = self.datadir
    self.proc = subprocess.Popen (args, env=envVars)

    self.rpc = self.createRpc ()
    self.env = env

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

    bestblk, bestheight = self.env.getChainTip ()

    while True:
      state = self.rpc.getcurrentstate ()
      if state["blockhash"] == bestblk:
        assert state["height"] == bestheight
        return state

      self.log.warning (("Channel daemon for %s has best block %s,"
                            + " waiting to catch up to %s")
          % (self.playerName, state["blockhash"], bestblk))
      time.sleep (0.01)


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

    # Ethereum account instances that have been constructed as signing
    # addresses (indexed by address).
    self.signerAccounts = {}

  def shutdown (self):
    self.broadcast.shutdown ()
    self.broadcastThread.join ()
    super (TestCase, self).shutdown ()

  @contextmanager
  def runChannelDaemon (self, playerName, **kwargs):
    """
    Starts a new channel daemon for the given player name and with
    the given extra arguments for the channel-daemon binary.

    The --playername, --rpc_port, --gsp_rpc_url and --broadcast_rpc_url
    flags are set automatically, all others are just forwarded from kwargs
    to the binary.

    This returns a context manager instance, which returns the
    underlying Daemon instance when entered.
    """

    daemon = Daemon (playerName, self.basedir, next (self.ports),
                     self.args.channel_daemon)

    daemon.start (self.env, playername=playerName,
                  gsp_rpc_url=self.gamenode.rpcurl,
                  broadcast_rpc_url=self.bcurl,
                  **kwargs)

    try:
      yield daemon
    finally:
      daemon.stop ()

  def newSigningAddress (self):
    """
    Returns a new address that can be used for signing on the channel.
    This constructs a new key pair, keeping the private key in memory
    and ready to be looked up when a daemon is started with that address.
    """

    account = Account.create ()
    self.signerAccounts[account.address] = account
    return account.address

  def getSigningProvider (self):
    """
    Returns a fake "RPC" provider implementing validateaddress, getaddressinfo
    and signmessage based on the signature addresses in this instance.
    This can then be used with signatures.createForChannel.
    """

    class FakeRpc:
      def __init__ (self, accounts):
        self.accounts = accounts

      def validateaddress (self, addr):
        return {"isvalid": True}

      def getaddressinfo (self, addr):
        return {"ismine": addr in self.accounts}

      def signmessage (self, addr, msg):
        assert addr in self.accounts, "Invalid signing address: %s" % addr
        encoded = messages.encode_defunct (text=msg)
        return self.accounts[addr].sign_message (encoded).signature

    return FakeRpc (self.signerAccounts)

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

  @contextmanager
  def waitForTurnIncrease (self, daemons, delta):
    """
    Runs a context that waits until the synced channel state between all
    daemons has increased the turn count by at least a certain number.
    """

    before = self.getSyncedChannelState (daemons)
    cntBefore = before["current"]["state"]["turncount"]

    yield

    while True:
      after = self.getSyncedChannelState (daemons)
      cntAfter = after["current"]["state"]["turncount"]

      if cntAfter >= cntBefore + delta:
        break

      self.log.warning ("Turn count still too small, waiting...")
      time.sleep (0.01)
