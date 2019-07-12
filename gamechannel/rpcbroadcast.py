# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
A JSON-RPC server that can be used as broadcasting system, in conjunction
with the RpcBroadcast client implementation.  Messages can be published
by calling RPC methods on the server, and also retrieved by long-polling
RPC calls.

Messages for each channel ID have sequence numbers (assigned by the server).
When a new participant joins the channel, they can request the current number.
Then, when requesting messages, client can specify the sequence number
starting from which they want to retrieve new messages.

Note that while this server is fully functional and can be used for
real game play, it is not optimised for production use yet (e.g. with
proper logging or DoS protection).  For instance, it does not attempt
to ever remove data of previously opened channels.  If we assume that active
clients will poll the server repeatedly, we can implement cleaning up of
old data (messages in an active channel as well as inactive channels) by
simply removing everything that has not been touched in a while.
"""

import logging
import threading

import SocketServer

from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer


class ThreadingJsonRpcServer (SocketServer.ThreadingMixIn, SimpleJSONRPCServer):
  pass


class Channel (object):
  """
  Data and logic for one particular broadcast channel in the server
  (as identified by its channel ID).
  """

  def __init__ (self):
    self.cv = threading.Condition ()

    # We simply keep track of all messages in a growing list.  The sequence
    # number of each message is its index plus one.  In other words, the
    # "current" sequence number in the beginning is zero.  When we add the
    # first message, the current sequence number is one, and so on.
    self.messages = []

  def send (self, msg):
    with self.cv:
      self.messages.append (msg)
      self.cv.notifyAll ()

  def getSeq (self):
    with self.cv:
      return len (self.messages)

  def receive (self, fromSeq, timeout):
    with self.cv:
      if len (self.messages) <= fromSeq:
        self.cv.wait (timeout)
      return self.messages[fromSeq:], len (self.messages)


class Server (object):
  """
  One instance of the server.  It holds all the data required for the
  running process, and provides the functionality to start and stop it
  as required.

  This class is used by the simple main method below, but it may also be
  used directly to start a server in an integration test (for instance).
  """

  # Timeout value for receive calls.
  RECEIVE_TIMEOUT = 3

  def __init__ (self, host, port):
    self.host = host
    self.port = port
    self.log = logging.getLogger ("rpcbroadcast.Server")
    self.server = ThreadingJsonRpcServer ((host, port))

    # We need to synchronise access to the dictionary of channels, so that
    # we can safely access and potentially add channels to it.  The mutex
    # only covers access to self.channels, though.  Once a channel is
    # retrieved, it will be released and the Channel instance will
    # be responsible for synchronisation of further work with it.
    self.channels = {}
    self.mutex = threading.Lock ()

    def sendMsg (channel, message):
      self.getChannel (channel).send (message)
    self.server.register_function (sendMsg, "send")

    def getSeq (channel):
      return {"seq": self.getChannel (channel).getSeq ()}
    self.server.register_function (getSeq, "getseq")

    def receiveMsg (channel, fromseq):
      ch = self.getChannel (channel)
      (msg, seq) = ch.receive (fromseq, self.RECEIVE_TIMEOUT)
      return {
        "messages": msg,
        "seq": seq,
      }
    self.server.register_function (receiveMsg, "receive")

  def getChannel (self, channel):
    """
    Retrieves the Channel instance for the given ID.  If it does not
    exist yet in our dictionary, we create a fresh channel.
    """

    with self.mutex:
      if channel not in self.channels:
        self.channels[channel] = Channel ()

      return self.channels[channel]

  def serve (self):
    """
    Starts serving with the server.  This function call blocks until the
    server is finished.
    """

    self.log.info ("Starting server at %s:%d..." % (self.host, self.port))
    self.server.serve_forever ()

  def shutdown (self):
    """
    Stops the running server (blocking until done).
    """

    self.log.info ("Shutting down server at %s:%d..." % (self.host, self.port))
    self.server.shutdown ()
