#!/usr/bin/env python3

# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from gamechannel import signatures

from gamechannel.proto import (metadata_pb2, signatures_pb2)

from google.protobuf import text_format

import base64
import unittest
import hashlib


class SignaturesTest (unittest.TestCase):

  def testChannelMessage (self):
    h = hashlib.sha256 ()
    h.update (b"channel id")
    channelId = h.digest ()

    meta = metadata_pb2.ChannelMetadata ()
    meta.reinit = b"re\0init"

    # This is logged by the C++ signatures test.
    msg = """Game-Channel Signature
Game ID: game id
Channel: 537e2fa2e3e5eafcb04ff353ff2a1984b7c1500419d322dcbec8b3613c224d57
Reinit: cmUAaW5pdA==
Topic: topic
Data Hash: d6b681bfce7155d44721afb79c296ef4f0fa80a9dd6b43c5cf74dd0f64c85512"""
    actual = signatures.getChannelMessage ("game id", channelId, meta,
                                           "topic", b"foo\0bar")
    self.assertEqual (actual, msg)

  def testCreateForChannel (self):
    class FakeRpc:
      addresses = ["addr 1", "addr 2"]

      def validateaddress (self, addr):
        return {"isvalid": True}

      def getaddressinfo (self, addr):
        return {"ismine": addr in self.addresses}

      def signmessage (self, addr, msg):
        for i in range (len (self.addresses)):
          if addr == self.addresses[i]:
            return b"sgn %d" % i
        raise AssertionError ("Invalid test address: %s" % addr)

    meta = metadata_pb2.ChannelMetadata ()
    text_format.Parse ("""
      reinit: "reinit"
      participants: { address: "addr 2" }
      participants: { address: "other address" }
    """, meta)

    rpc = FakeRpc ()
    channel = {
      "id": "ab" * 32,
      "meta":
        {
          "proto": base64.b64encode (meta.SerializeToString ()),
        },
    }

    expected = signatures_pb2.SignedData ()
    text_format.Parse ("""
      data: "foobar"
      signatures: "sgn 1"
    """, expected)

    actual = signatures.createForChannel (rpc, "game id", channel,
                                          "topic", b"foobar")
    self.assertEqual (actual, expected)


if __name__ == "__main__":
  unittest.main ()
