#!/usr/bin/env python3
# Copyright (C) 2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from mover import MoverTest

import asyncio
from contextlib import contextmanager
import json
import os
import os.path
import subprocess
import sys
import time

from websockets.asyncio.client import connect as websocketConnect


"""
Tests basic notification forwarding of the GSP websocket server.
"""


class BasicTest (MoverTest):
  """
  Runs a real GSP and the real websocket server, and checks that new-block
  and pending-move notifications are forwarded to a connected client.
  """

  def serverScript (self):
    basedir = os.path.dirname (os.path.abspath (__file__))
    return os.path.abspath (os.path.join (basedir, "..",
                                          "gsp-websocket-server.py"))

  @contextmanager
  def websocketServer (self):
    port = next (self.ports)
    logPath = os.path.join (self.basedir, "websocket.log")

    args = [
      sys.executable,
      self.serverScript (),
      "--host=127.0.0.1",
      "--port=%d" % port,
      "--gsp_rpc_url=%s" % self.gamenode.rpcurl,
      "--enable_pending",
      "--retry_ms=1000",
    ]

    self.log.info ("Starting websocket server: %s" % " ".join (args))
    with open (logPath, "w", buffering=1) as log:
      proc = subprocess.Popen (args, stdout=log, stderr=subprocess.STDOUT)
      try:
        yield proc, "ws://127.0.0.1:%d" % port
      finally:
        proc.terminate ()
        try:
          proc.wait (timeout=5)
        except subprocess.TimeoutExpired:
          proc.kill ()
          proc.wait ()

  async def connect (self, proc, url):
    deadline = time.monotonic () + 10
    while True:
      if proc.poll () is not None:
        raise AssertionError ("Websocket server exited with status %d"
                                % proc.returncode)

      try:
        return await websocketConnect (url)
      except Exception:
        if time.monotonic () >= deadline:
          raise
        await asyncio.sleep (0.1)

  async def waitForNotification (self, websocket, method, predicate):
    deadline = time.monotonic () + 10
    while True:
      remaining = deadline - time.monotonic ()
      if remaining <= 0:
        raise AssertionError ("Timeout waiting for %s notification" % method)

      msg = json.loads (await asyncio.wait_for (websocket.recv (),
                                                timeout=remaining))
      if msg.get ("method") != method:
        continue
      if predicate (msg):
        return msg

  async def testNotifications (self, proc, url):
    oldPending = self.rpc.game.getpendingstate ()
    oldVersion = oldPending["version"]

    websocket = await self.connect (proc, url)
    try:
      self.generate (1)
      expectedBlock = self.rpc.xaya.getbestblockhash ()
      msg = await self.waitForNotification (
          websocket, "newblock",
          lambda m: m["params"][0] == expectedBlock)
      self.assertEqual (msg, {
        "jsonrpc": "2.0",
        "method": "newblock",
        "params": [expectedBlock],
      })

      self.move ("a", "k", 2)
      msg = await self.waitForNotification (
          websocket, "pendingupdate",
          lambda m: m["params"][0]["version"] > oldVersion
                    and "a" in m["params"][0]["pending"])
      self.assertEqual (msg["jsonrpc"], "2.0")
      self.assertEqual (msg["method"], "pendingupdate")
      assert "a" in msg["params"][0]["pending"]
    finally:
      await websocket.close ()

  def run (self):
    self.generate (101)
    self.syncGame ()

    with self.websocketServer () as (proc, url):
      asyncio.run (self.testNotifications (proc, url))


if __name__ == "__main__":
  BasicTest ().main ()
