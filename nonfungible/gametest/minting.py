#!/usr/bin/env python3
# coding=utf8

# Copyright (C) 2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests minting of assets.
"""

from nftest import NonFungibleTest


class MintingTest (NonFungibleTest):

  def run (self):
    self.collectPremine ()
    self.assertEqual (self.getRpc ("listassets"), [])

    self.sendMove ("domob", [
      {"m": {"a": "foo", "n": 10}},
      {"m": {"a": "invalid\nname", "n": 1}},
      {"m": {"a": "bar", "n": 0, "d": "custom data"}},
      {"m": {"a": "bar", "n": 100}},
    ])
    self.sendMove ("", {"m": {"a": "", "n": 0}})
    self.sendMove (u"äöü", {"m": {"a": u"ß", "n": 42}})
    self.generate (1)

    self.assertEqual (self.getRpc ("listassets"), [
      {"m": "", "a": ""},
      {"m": "domob", "a": "bar"},
      {"m": "domob", "a": "foo"},
      {"m": u"äöü", "a": u"ß"},
    ])

    self.assertEqual (self.getRpc ("getbalance",
                                   name=u"äöü", asset={"m": u"äöü", "a": u"ß"}),
                      42)
    self.assertEqual (self.getRpc ("getbalance",
                                   name="invalid",
                                   asset={"m": "foo", "a": "bar"}),
                      0)

    self.assertEqual (self.getRpc ("getassetdetails",
                                   {"m": "domob", "a": "foo"}),
                      {
      "asset": {"m": "domob", "a": "foo"},
      "supply": 10,
      "data": None,
      "balances": {"domob": 10},
    })
    self.assertEqual (self.getRpc ("getassetdetails",
                                   {"m": "domob", "a": "bar"}),
                      {
      "asset": {"m": "domob", "a": "bar"},
      "supply": 0,
      "data": "custom data",
      "balances": {},
    })

    self.assertEqual (self.getRpc ("getuserbalances", "domob"), [
      {
        "asset": {"m": "domob", "a": "foo"},
        "balance": 10,
      },
    ])


if __name__ == "__main__":
  MintingTest ().main ()
