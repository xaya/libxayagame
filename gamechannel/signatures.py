# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Utility methods for working with SignedData objects from game channels.
"""

from .proto import (metadata_pb2, signatures_pb2)

import base64
import codecs
import hashlib


def getChannelMessage (gameId, channelId, meta, topic, data):
  """
  Returns the raw message that is signed (as string, using signmessage)
  for signing the given data in a channel with the given ID and topic.

  The channelId is given as 32 raw bytes that encode the uint256 directly
  (in big endian, i.e. the same way it is represented in hex).
  """

  assert len (channelId) == 32

  reinit = codecs.decode (base64.b64encode (meta.reinit), "ascii")

  return "Game-Channel Signature\n" \
          + ("Game ID: %s\n" % gameId) \
          + ("Channel: %s\n" % channelId.hex ()) \
          + ("Reinit: %s\n" % reinit) \
          + ("Topic: %s\n" % topic) \
          + ("Data Hash: %s" % hashlib.sha256 (data).hexdigest ())


def createForChannel (rpc, gameId, channel, topic, data):
  """
  Constructs a SignedData protocol buffer instance that contains the given
  data and signatures for all participants of the channel for which the private
  key is known through the RPC connection.

  channel should be an object similar to how game channels are represented
  in the game state JSON.  In particular, it should have an "id" field with
  the channel ID as hex, and meta.proto should be the base64-encoded serialised
  metadata proto.
  """

  res = signatures_pb2.SignedData ()
  res.data = data

  channelId = codecs.decode (channel["id"], "hex")
  meta = metadata_pb2.ChannelMetadata ()
  meta.ParseFromString (base64.b64decode (channel["meta"]["proto"]))
  msg = getChannelMessage (gameId, channelId, meta, topic, data)

  for p in meta.participants:
    valid = rpc.validateaddress (p.address)
    if not valid["isvalid"]:
      continue
    info = rpc.getaddressinfo (p.address)
    if info["ismine"]:
      sgn = rpc.signmessage (p.address, msg)
      res.signatures.append (sgn)

  return res
