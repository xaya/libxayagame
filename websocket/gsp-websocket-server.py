#!/usr/bin/env python3

# Copyright (C) 2021-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
from contextlib import contextmanager
import json
import logging
import threading

import jsonrpclib
from websocket_server import WebsocketServer


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
      while True:
        with mut:
          if shouldStop:
            return
        new = rpc (known)
        if new != known:
          pusher (new)
          known = new

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
  args = parser.parse_args ()

  srv = Server (args.host, args.port, args.gsp_rpc_url)
  if args.enable_pending:
    srv.enablePending ()
  srv.run ()
