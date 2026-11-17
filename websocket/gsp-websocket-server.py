#!/usr/bin/env python3

# Copyright (C) 2021-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
import asyncio
from contextlib import contextmanager
import json
import logging
import threading
import time

import jsonrpclib
import websockets
from websockets.asyncio.server import serve as websocketServe


# Time (in seconds) to sleep between checks for "should stop"
# while waiting for a retry.  This is the latency that killing the
# process might incur.
SLEEP_TIME = 0.1

# Default maximum number of queued messages per client.  Clients that fall
# further behind than this are disconnected.
DEFAULT_QUEUE_SIZE = 1000

# Time (in seconds) to wait for closing a connection before websockets
# aborts it.
CLOSE_TIMEOUT = 1


class Server:
  """
  A Websocket server that polls a GSP's waitforchange and waitforpendingchange
  RPC interfaces and pushes notifications to connected clients.
  """

  def __init__ (self, host, port, gspRpcUrl, clientQueueSize, retryMs):
    self.host = host
    self.port = port
    self.rpcUrl = gspRpcUrl
    self.clientQueueSize = clientQueueSize
    self.retryMs = retryMs
    self.log = logging.getLogger ("gsp-websocket-server")
    self.withPending = False

    self.loop = None
    self.clients = {}
    self.nextClientId = 1

  def enablePending (self):
    self.withPending = True
    self.log.info ("Pending moves enabled")

  def run (self):
    """
    Starts the server, GSP polling thread and blocks forever while
    the server is running.
    """

    asyncio.run (self._run ())

  async def _run (self):
    self.loop = asyncio.get_running_loop ()

    try:
      with self.pollingWorker ():
        async with websocketServe (self._handleClient, self.host, self.port,
                                   close_timeout=CLOSE_TIMEOUT):
          self.log.info ("Listening on %s:%d" % (self.host, self.port))
          await asyncio.Event ().wait ()
    finally:
      await self._closeAllClients ()

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
          toSleep = self.retryMs / 1_000.0

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
    self._pushMessage (msg)

  def pushPendingUpdate (self, data):
    data = {
      "jsonrpc": "2.0",
      "method": "pendingupdate",
      "params": [data],
    }
    msg = json.dumps (data, separators=(",", ":"))
    self._pushMessage (msg)

  def _pushMessage (self, msg):
    """
    Enqueues a message for all clients from a polling thread.
    """

    loop = self.loop
    if loop is None:
      return

    try:
      loop.call_soon_threadsafe (self._broadcast, msg)
    except RuntimeError:
      # The event loop has already been closed.
      pass

  def _broadcast (self, msg):
    """
    Enqueues a message for all clients.  This runs in the event loop.
    """

    for client in list (self.clients.values ()):
      try:
        client["queue"].put_nowait (msg)
      except asyncio.QueueFull:
        self._dropClient (client)

  def _dropClient (self, client):
    """
    Drops a slow client whose queue is full.  This runs in the event loop.
    """

    if self.clients.pop (client["id"], None) is None:
      return

    self.log.warning ("Dropping slow client %d" % client["id"])

    writer = client["writer"]
    if writer is not None:
      writer.cancel ()

    asyncio.create_task (self._closeConnection (client, 1013,
                                                "client too slow"))

  async def _handleClient (self, websocket):
    """
    Handles a new websocket connection.
    """

    client = {
      "id": self.nextClientId,
      "websocket": websocket,
      "address": getattr (websocket, "remote_address", None),
      "queue": asyncio.Queue (maxsize=self.clientQueueSize),
      "writer": None,
    }
    self.nextClientId += 1
    self.clients[client["id"]] = client

    client["writer"] = asyncio.create_task (self._writerLoop (client))
    self.log.info ("Client %d connected from %s"
                     % (client["id"], client["address"]))

    try:
      async for _ in websocket:
        pass
    except asyncio.CancelledError:
      raise
    except Exception:
      pass
    finally:
      await self._removeClient (client)
      self.log.info ("Client %d disconnected" % client["id"])

  async def _writerLoop (self, client):
    """
    Sends queued messages to one client.
    """

    try:
      while True:
        msg = await client["queue"].get ()
        await client["websocket"].send (msg)
    except asyncio.CancelledError:
      pass
    except websockets.ConnectionClosed:
      await self._removeClient (client)
    except Exception as exc:
      self.log.warning ("Sending to client %d failed: %s"
                          % (client["id"], exc))
      await self._removeClient (client)

  async def _removeClient (self, client):
    """
    Removes a client and closes its connection.
    """

    if self.clients.pop (client["id"], None) is None:
      return

    writer = client["writer"]
    if writer is not None and writer is not asyncio.current_task ():
      writer.cancel ()

    await self._closeConnection (client, 1000, "")

  async def _closeConnection (self, client, code, reason):
    try:
      await client["websocket"].close (code=code, reason=reason)
    except asyncio.CancelledError:
      pass
    except Exception:
      pass

  async def _closeAllClients (self):
    clients = list (self.clients.values ())
    self.clients.clear ()

    writers = []
    for client in clients:
      writer = client["writer"]
      if writer is not None and writer is not asyncio.current_task ():
        writer.cancel ()
        writers.append (writer)

    await asyncio.gather (
        *(self._closeConnection (c, 1001, "server shutting down")
          for c in clients),
        return_exceptions=True)

    if writers:
      await asyncio.gather (*writers, return_exceptions=True)


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
  parser.add_argument ("--client_queue_size", type=int,
                       default=DEFAULT_QUEUE_SIZE,
                       help="Maximum queued messages per client before"
                            " dropping it as too slow")
  args = parser.parse_args ()

  logging.basicConfig (level=logging.INFO,
                       format="%(asctime)s %(levelname)s: %(message)s")

  srv = Server (args.host, args.port, args.gsp_rpc_url,
                clientQueueSize=args.client_queue_size,
                retryMs=args.retry_ms)
  if args.enable_pending:
    srv.enablePending ()
  srv.run ()
