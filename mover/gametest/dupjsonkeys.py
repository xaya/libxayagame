#!/usr/bin/env python3
# Copyright (C) 2018-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests how the GSP behaves if there are duplicated JSON keys in moves
from Xaya Core.  (Since univalue, as used to validate moves in Xaya Core
and send notifications, accepts duplicated keys.)
"""

from mover import MoverTest

import json


class DupJsonKeysTest (MoverTest):

  def run (self):
    self.generate (101)
    self.expectGameState ({"players": {}})

    self.rpc.xaya.name_register ("p/test", "{}")
    self.generate (1)

    # Try a duplicated game ID in the move JSON.  In this case, the notification
    # sent on ZMQ should just include the last value (that is what Xaya Core
    # officially does, enforced by a test).
    mv1 = json.dumps ({"d": "k", "n": 1})
    mv2 = json.dumps ({"d": "j", "n": 1})
    self.rpc.xaya.name_update ("p/test", '{"g":{"mv":%s,"mv":%s}}' % (mv1, mv2))
    self.generate (1)
    self.expectGameState ({"players": {
      "test": {"x": 0, "y": -1},
    }})

    # Try sending a move where the move itself contains duplicated keys.
    # For this situation, libxayagame dedups the key (based on JsonCpp's
    # parsing) and returns the last value only.
    self.move ("a", "l", 1)
    mv = '{"d":"h","d":"l","n":1}'
    self.rpc.xaya.name_update ("p/test", '{"g":{"mv":%s}}' % mv)
    self.move ("z", "h", 1)
    self.generate (1)
    self.expectGameState ({"players": {
      "test": {"x": 1, "y": -1},
      "a": {"x": 1, "y": 0},
      "z": {"x": -1, "y": 0},
    }})


if __name__ == "__main__":
  DupJsonKeysTest ().main ()
