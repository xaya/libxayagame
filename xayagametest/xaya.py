# Copyright (C) 2018-2021 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Code for running the Xaya Core daemon as component in an integration test.
"""

from contextlib import contextmanager
import jsonrpclib
import logging
import os
import os.path
import shutil
import subprocess
import time


class Node:
  """
  An instance of the Xaya Core daemon that is running in regtest mode and
  used as component in an integration test of a Xaya game.
  """

  def __init__ (self, basedir, rpcPort, zmqPorts, binary):
    self.log = logging.getLogger ("xayagametest.xayanode")
    self.datadir = os.path.join (basedir, "xayanode")
    self.binary = binary

    self.config = {
      "listen": False,
      "rpcuser": "xayagametest",
      "rpcpassword": "xayagametest",
      "rpcport": rpcPort,
      "fallbackfee": 0.001,
    }
    for name, port in zmqPorts.items ():
      self.config["zmqpub%s" % name] = "tcp://127.0.0.1:%d" % port
    self.baseRpcUrl = ("http://%s:%s@localhost:%d"
        % (self.config["rpcuser"], self.config["rpcpassword"],
           self.config["rpcport"]))

    self.log.info ("Creating fresh data directory for Xaya node in %s"
                    % self.datadir)
    shutil.rmtree (self.datadir, ignore_errors=True)
    os.mkdir (self.datadir)
    with open (os.path.join (self.datadir, "xaya.conf"), "wt") as f:
      f.write ("[regtest]\n")
      for key, value in self.config.items ():
        f.write ("%s=%s\n" % (key, value))

    self.proc = None

  def start (self):
    if self.proc is not None:
      self.log.error ("Xaya process is already running, not starting again")
      return

    self.log.info ("Starting new Xaya process")
    args = [self.binary]
    args.append ("-datadir=%s" % self.datadir)
    args.append ("-noprinttoconsole")
    args.append ("-regtest")
    self.proc = subprocess.Popen (args)

    # Start with a temporary RPC connection without wallet, which we
    # use to make sure the default wallet is created / loaded.
    rpc = jsonrpclib.ServerProxy (self.baseRpcUrl)

    self.log.info ("Waiting for the JSON-RPC server to be up...")
    while True:
      try:
        data = rpc.getnetworkinfo ()
        self.log.info ("Daemon %s is up" % data["subversion"])
        break
      except:
        time.sleep (0.1)

    # Make sure we have a default wallet.
    wallets = rpc.listwallets ()
    if "" not in wallets:
      self.log.info ("Creating default wallet in Xaya Core...")
      rpc.createwallet ("")

    # We need to explicitly close the client connection, or else
    # Xaya Core will wait for it when shutting down.
    rpc ("close") ()

    self.rpcHandles = []
    self.rpcurl, self.rpc = self.getWalletRpc ("")

  def stop (self):
    if self.proc is None:
      self.log.error ("No Xaya process is running cannot stop it")
      return

    self.log.info ("Stopping Xaya process")
    self.rpc.stop ()

    for h in self.rpcHandles:
      h ("close") ()
    self.rpcHandles = []

    self.log.info ("Waiting for Xaya process to stop...")
    self.proc.wait ()
    self.proc = None

  @contextmanager
  def run (self):
    """
    Runs the Xaya node with a context manager.
    """

    self.start ()
    try:
      yield self
    finally:
      self.stop ()

  def getWalletRpc (self, wallet):
    """
    Returns the RPC URL to use for a particular wallet as well as
    a ServerProxy instance.
    """

    url = "%s/wallet/%s" % (self.baseRpcUrl, wallet)
    rpc = jsonrpclib.ServerProxy (url)

    # Record all RPC handles created, so we can close them when
    # shutting down.
    self.rpcHandles.append (rpc)

    return url, rpc


class Environment:
  """
  A "base-chain environment" that consists just of a Xaya Core instance.

  This is an abstraction that can be used for testing in situations where
  a GSP is run not connected directly to Xaya Core, but for instance
  to a Xaya X instance and some other base chain.  In the present case, however,
  it is just a simple wrapper around a normal Xaya Core node.
  """

  def __init__ (self, *args, **kwargs):
    self.node = Node (*args, **kwargs)

  @contextmanager
  def run (self):
    """
    Runs the environment in a context.
    """

    with self.node.run ():
      yield self

  def generate (self, num):
    """
    Generates num new blocks on the active chain, and returns their hashes
    in an array.
    """

    addr = self.node.rpc.getnewaddress ()
    return self.node.rpc.generatetoaddress (num, addr)

  def getChainTip (self):
    """
    Returns the hash and height of the current chain tip according
    to the base chain (not the GSP).
    """

    info = self.node.rpc.getblockchaininfo ()
    return info["bestblockhash"], info["blocks"]

  def nameExists (self, ns, nm):
    """
    Returns true if the given namespace/name combination exists, and false
    if it does not.
    """

    full = "%s/%s" % (ns, nm)
    if self.node.rpc.name_pending (full):
      return True

    try:
      self.node.rpc.name_show (full)
      return True
    except Exception as exc:
      return False

  def register (self, ns, nm):
    """
    Registers a new name with the given namespace and base name.  Returns the
    transaction ID.
    """

    return self.node.rpc.name_register ("%s/%s" % (ns, nm))

  def move (self, ns, nm, strval, options={}):
    """
    Sends a move with the given name, which is supposed to be existing
    already.  The move data is already encoded as a string.
    Returns the transaction ID.

    Arguments beyond strval are non-standard and can be specific to
    a particular environment.
    """

    return self.node.rpc.name_update ("%s/%s" % (ns, nm), strval, options)

  def getGspArguments (self):
    """
    Returns the Xaya RPC URL and any potential extra arguments that should
    be used to link a GSP to this environment.
    """

    return self.node.rpcurl, []
