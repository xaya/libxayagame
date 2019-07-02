#!/usr/bin/env python

# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import signatures

from proto import (metadata_pb2, signatures_pb2)

from google.protobuf import text_format

import base64
import unittest
import hashlib


class SignaturesTest (unittest.TestCase):

  def testChannelMessage (self):
    h = hashlib.sha256 ()
    h.update ("channel id")
    channelId = h.digest ()

    meta = metadata_pb2.ChannelMetadata ()
    meta.reinit = "re\0init"

    # This is logged by the C++ signatures test.
    msg = "917ad3494da16c7728ef5f8f44f2285d7d7fd3ed7b78278be440fa644927d5cc"
    actual = signatures.getChannelMessage (channelId, meta, "topic", "foo\0bar")
    self.assertEqual (actual, msg)

  def testCreateForChannel (self):
    class FakeRpc:
      addresses = ["addr 1", "addr 2"]

      def validateaddress (self, addr):
        return {"isvalid": True}

      def getaddressinfo (self, addr):
        for a in self.addresses:
          if a == addr:
            return {"ismine": True}
        return {"ismine": False}

      def signmessage (self, addr, msg):
        for i in range (len (self.addresses)):
          if addr == self.addresses[i]:
            return base64.b64encode ("sgn %d" % i)
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

    actual = signatures.createForChannel (rpc, channel, "topic", "foobar")
    self.assertEqual (actual, expected)


if __name__ == "__main__":
  unittest.main ()
