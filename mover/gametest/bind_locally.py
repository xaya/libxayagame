#!/usr/bin/env python3
# Copyright (C) 2019-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

import errno
import jsonrpclib
import socket

"""
Tests that the game daemon listens only locally by default, but can be
changed to listen on all interfaces as well.
"""


class BindAddressTest (MoverTest):

  def run (self):
    port = self.gamenode.port
    rpc = jsonrpclib.ServerProxy ("http://127.0.0.1:%d" % port)
    alternateRpc = jsonrpclib.ServerProxy ("http://127.0.0.2:%d" % port)

    # By default, the normal RPC connection to 127.0.0.1 should work.  But the
    # connection to the alternate IP 127.0.0.2 should fail.
    assert rpc.getcurrentstate ()["chain"] == "regtest"
    try:
      alternateRpc.getcurrentstate ()
      raise AssertionError ("Expected connection failure, got none")
    except socket.error as exc:
      assert exc.errno == errno.ECONNREFUSED

    # Restart and listen on all interfaces.
    self.stopGameDaemon ()
    self.startGameDaemon (extraArgs=["--game_rpc_listen_locally=false"])

    # Now both connections should work.
    assert rpc.getcurrentstate ()["chain"] == "regtest"
    assert alternateRpc.getcurrentstate ()["chain"] == "regtest"


if __name__ == "__main__":
  BindAddressTest ().main ()
