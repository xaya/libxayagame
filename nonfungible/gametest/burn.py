#!/usr/bin/env python3

# Copyright (C) 2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests burning of coins.
"""

from nftest import NonFungibleTest


class BurnTest (NonFungibleTest):

  def run (self):
    self.collectPremine ()

    self.sendMove ("domob", [
      {"m": {"a": "foo", "n": 10}},
      {"b": {"a": {"m": "domob", "a": "foo"}, "n": 11}},
      {"b": {"a": {"m": "domob", "a": "foo"}, "n": 9}},
    ])
    self.generate (1)
    self.assertEqual (self.getRpc ("getassetdetails",
                                   {"m": "domob", "a": "foo"}),
                      {
      "asset": {"m": "domob", "a": "foo"},
      "supply": 1,
      "data": None,
      "balances": {"domob": 1},
    })

    self.sendMove ("domob", [
      {"b": {"a": {"m": "domob", "a": "foo"}, "n": 1}},
    ])
    self.generate (1)
    self.assertEqual (self.getRpc ("getuserbalances", "domob"), [])
    self.assertEqual (self.getRpc ("getassetdetails",
                                   {"m": "domob", "a": "foo"}),
                      {
      "asset": {"m": "domob", "a": "foo"},
      "supply": 0,
      "data": None,
      "balances": {},
    })


if __name__ == "__main__":
  BurnTest ().main ()
