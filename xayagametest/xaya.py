# Copyright (C) 2018-2021 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Code for running the Xaya Core daemon as component in an integration test.
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
      "zmqpubgameblocks": "tcp://127.0.0.1:%d" % zmqPorts["blocks"],
    }
    if "pending" in zmqPorts:
      zmqPending = "tcp://127.0.0.1:%d" % zmqPorts["pending"]
      self.config["zmqpubgamepending"] = zmqPending
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
        time.sleep (1)

    # Make sure we have a default wallet.
    wallets = rpc.listwallets ()
    if "" not in wallets:
      self.log.info ("Creating default wallet in Xaya Core...")
      rpc.createwallet ("")

    # We need to explicitly close the client connection, or else
    # Xaya Core will wait for it when shutting down.
    rpc ("close") ()

    self.rpcurl, self.rpc = self.getWalletRpc ("")

  def stop (self):
    if self.proc is None:
      self.log.error ("No Xaya process is running cannot stop it")
      return

    self.log.info ("Stopping Xaya process")
    self.rpc.stop ()

    self.log.info ("Waiting for Xaya process to stop...")
    self.proc.wait ()
    self.proc = None

  def getWalletRpc (self, wallet):
    """
    Returns the RPC URL to use for a particular wallet as well as
    a ServerProxy instance.
    """

    url = "%s/wallet/%s" % (self.baseRpcUrl, wallet)
    return url, jsonrpclib.ServerProxy (url)
