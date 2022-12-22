#!/usr/bin/env python3

# Copyright (C) 2021-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
from contextlib import contextmanager
import json
import logging
import threading
import time

import jsonrpclib
from websocket_server import WebsocketServer


# Time (in seconds) to sleep between checks for "should stop"
# while waiting for a retry.  This is the latency that killing the
# process might incur.
SLEEP_TIME = 0.1


class Server:
  """
  A Websocket server that polls a GSP's waitforchange and waitforpendingchange
  RPC interfaces and pushes notifications to connected clients.
  """

  def __init__ (self, host, port, gspRpcUrl):
    self.srv = WebsocketServer (host=host, port=port, loglevel=logging.INFO)
    self.rpcUrl = gspRpcUrl
    self.log = logging.Logger ("")
    self.withPending = False

  def enablePending (self):
    self.withPending = True
    self.log.info ("Pending moves enabled")

  def run (self):
    """
    Starts the server, GSP polling thread and blocks forever while
    the server is running.
    """

    with self.pollingWorker ():
      self.srv.run_forever ()

  @contextmanager
  def pollingWorker (self):
    """
    Runs a new polling worker task in a managed context.  When the context
    is exited, the worker is stopped.
    """

    mut = threading.Lock ()
    shouldStop = False

    def loop (rpc, firstKnown, pusher):
      known = firstKnown
      toSleep = 0
      while True:
        with mut:
          if shouldStop:
            return

        if toSleep > 0:
          # Sleep only a shorter duration at a time, so that we can detect
          # a shutdown request in-between.
          time.sleep (SLEEP_TIME)
          toSleep -= SLEEP_TIME
          continue

        try:
          new = rpc (known)
          if new != known:
            pusher (new)
            known = new
        except Exception as exc:
          # In case of an error (RPC or websocket sending), we wait for a
          # predefined time before retrying again.
          self.log.exception (exc)
          toSleep = args.retry_ms / 1_000.0

    threads = []

    rpcBlocks = jsonrpclib.ServerProxy (self.rpcUrl)
    threads.append (threading.Thread (target=loop, args=(
        rpcBlocks.waitforchange, "", self.pushNewBlock
    )))

    if self.withPending:
      rpcPending = jsonrpclib.ServerProxy (self.rpcUrl)
      # Closure wrapping around the waitforpendingchange RPC, so that it
      # accepts as argument a JSON object with version (rather than just the
      # version), such as it returns.  This makes it fit into
      # the same schema as waitforchange, and is all we need.
      def waitForPending (data):
        return rpcPending.waitforpendingchange (data["version"])
      threads.append (threading.Thread (target=loop, args=(
          waitForPending, {"version": 0}, self.pushPendingUpdate
      )))

    for t in threads:
      t.start ()

    try:
      yield
    finally:
      with mut:
        shouldStop = True
      for t in threads:
        t.join ()

  def pushNewBlock (self, blockHash):
    data = {
      "jsonrpc": "2.0",
      "method": "newblock",
      "params": [blockHash],
    }
    msg = json.dumps (data, separators=(",", ":"))
    self.srv.send_message_to_all (msg)

  def pushPendingUpdate (self, data):
    data = {
      "jsonrpc": "2.0",
      "method": "pendingupdate",
      "params": [data],
    }
    msg = json.dumps (data, separators=(",", ":"))
    self.srv.send_message_to_all (msg)


if __name__ == "__main__":
  desc = "Runs a Websocket server that forwards notifications from a GSP to" \
         " connected clients."
  parser = argparse.ArgumentParser (description=desc)
  parser.add_argument ("--port", required=True, type=int,
                       help="Port to listen on for Websocket connections")
  parser.add_argument ("--host", default="0.0.0.0",
                       help="Host to listen on for Websocket connections")
  parser.add_argument ("--gsp_rpc_url", required=True,
                       help="URL for the GSP's JSON-RPC interface")
  parser.add_argument ("--enable_pending", action="store_true",
                       help="Also track and push updates to pending moves")
  parser.add_argument ("--retry_ms", type=int, default=10_000,
                       help="Time to wait before retrying on errors")
  args = parser.parse_args ()

  srv = Server (args.host, args.port, args.gsp_rpc_url)
  if args.enable_pending:
    srv.enablePending ()
  srv.run ()
