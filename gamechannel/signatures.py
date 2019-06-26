# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Utility methods for working with SignedData objects from game channels.
"""

from proto import signatures_pb2

import base64
import hashlib


def getChannelMessage (channelId, topic, data):
  """
  Returns the raw message that is signed (as string, using signmessage)
  for signing the given data in a channel with the given ID and topic.

  The channelId is given as 32 raw bytes that encode the uint256 directly
  (in big endian, i.e. the same way it is represented in hex).
  """

  assert len (channelId) == 32
  hasher = hashlib.sha256 ()

  hasher.update (channelId)
  hasher.update (topic)
  hasher.update ("\0")

  hasher.update (data)

  return hasher.digest ().encode ("hex")


def createForChannel (rpc, channel, topic, data):
  """
  Constructs a SignedData protocol buffer instance that contains the given
  data and signatures for all participants of the channel for which the private
  key is known through the RPC connection.

  channel should be an object similar to how game channels are represented
  in the game state JSON.  In particular, it should have an "id" field with
  the channel ID as hex, and meta.participants with the participants and their
  signing addresses.
  """

  res = signatures_pb2.SignedData ()
  res.data = data

  channelId = channel["id"].decode ("hex")
  msg = getChannelMessage (channelId, topic, data)

  for p in channel["meta"]["participants"]:
    valid = rpc.validateaddress (p["address"])
    if not valid["isvalid"]:
      continue
    info = rpc.getaddressinfo (p["address"])
    if info["ismine"]:
      sgn = rpc.signmessage (p["address"], msg)
      res.signatures.append (base64.b64decode (sgn))

  return res
