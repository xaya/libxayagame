#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Integration test for the JSON-RPC broadcast method.  This starts up a local
server and then runs the test binary pointed to it.
"""

import rpcbroadcast

import os
import os.path
import subprocess
import sys
import threading


class DetachedBroadcastServer (threading.Thread):

  PORT = 32500

  def __init__ (self):
    super (DetachedBroadcastServer, self).__init__ ()
    self.server = rpcbroadcast.Server ("localhost", self.PORT)

  def run (self):
    self.server.serve ()

  def stop (self):
    self.server.shutdown ()
    self.join ()


if __name__ == "__main__":
  server = DetachedBroadcastServer ()

  builddir = os.getenv ("builddir")
  if builddir is None:
    builddir = "."
  testbin = os.path.join (builddir, "test_rpcbroadcast")

  url = "--rpc_url=http://localhost:%d" % server.PORT

  try:
    server.start ()
    sys.exit (subprocess.call ([testbin, url]))
  finally:
    server.stop ()
