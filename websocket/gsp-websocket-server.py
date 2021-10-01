#!/usr/bin/env python3

# Copyright (C) 2021 The Xaya developers
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
  A Websocket server that polls a GSP's waitforchange RPC interface and
  pushes notifications to connected clients.
  """

  def __init__ (self, host, port, gspRpcUrl):
    self.srv = WebsocketServer (host=host, port=port, loglevel=logging.INFO)
    self.rpc = jsonrpclib.ServerProxy (gspRpcUrl)

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

    def loop ():
      knownBlock = ""
      while True:
        newBlock = self.rpc.waitforchange (knownBlock)
        if newBlock != knownBlock:
          self.pushNewBlock (newBlock)
          knownBlock = newBlock
        with mut:
          if shouldStop:
            return

    thread = threading.Thread (target=loop)
    thread.start ()

    try:
      yield
    finally:
      with mut:
        shouldStop = True
      thread.join ()

  def pushNewBlock (self, blockHash):
    data = {
      "jsonrpc": "2.0",
      "method": "newblock",
      "params": [blockHash],
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
  args = parser.parse_args ()

  srv = Server (args.host, args.port, args.gsp_rpc_url)
  srv.run ()
