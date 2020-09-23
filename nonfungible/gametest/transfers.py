#!/usr/bin/env python3

# Copyright (C) 2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests coin transfers.
"""

from nftest import NonFungibleTest


class TransfersTest (NonFungibleTest):

  def run (self):
    self.collectPremine ()

    self.sendMove ("domob", [
      {"m": {"a": "foo", "n": 10}},
      {"m": {"a": "bar", "n": 10}},
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 5, "r": "andy"}},
    ])
    self.sendMove ("andy", [
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 10, "r": "wrong"}},
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 3, "r": "andy"}},
      {"t": {"a": {"m": "domob", "a": "foo"}, "n": 3, "r": "daniel"}},
    ])
    self.generate (1)

    self.assertEqual (self.getRpc ("getassetdetails",
                                   {"m": "domob", "a": "foo"}),
                      {
      "asset": {"m": "domob", "a": "foo"},
      "supply": 10,
      "data": None,
      "balances": {"domob": 5, "andy": 2, "daniel": 3},
    })


if __name__ == "__main__":
  TransfersTest ().main ()
