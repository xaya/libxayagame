#!/usr/bin/env python3
# Copyright (C) 2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from shipstest import ShipsTest

from xayax.eth import uintToXaya

from solcx import compile_source

"""
Tests how channel management works if multiple moves or "changing"
moves are sent through a single transaction ID.
"""


CODE = """
  pragma solidity ^0.8.4;

  interface IXayaAccounts
  {
    function move (string memory ns, string memory name, string memory mv,
                   uint256 nonce, uint256 amount, address receiver)
        external returns (uint256);
  }

  /**
   * @dev A simple helper contract, which allows sending two moves
   * from a single transaction, as well as sending one of the other
   * (based on the same transaction) depending on state.
   */
  contract TwoMoves
  {

    IXayaAccounts public immutable accounts;

    bool public sendFirst;
    bool public sendSecond;

    constructor (IXayaAccounts acc)
    {
      accounts = acc;
    }

    function setFlags (bool first, bool second) public
    {
      sendFirst = first;
      sendSecond = second;
    }

    function send (string memory ns1, string memory name1, string memory mv1,
                   string memory ns2, string memory name2, string memory mv2)
        public
    {
      if (sendFirst)
        accounts.move (ns1, name1, mv1, type (uint256).max, 0, address (0));
      if (sendSecond)
        accounts.move (ns2, name2, mv2, type (uint256).max, 0, address (0));
    }

  }
"""


class MultiMovesTest (ShipsTest):

  def run (self):
    compiled = compile_source (CODE)
    _, data = compiled.popitem ()
    two = self.env.ganache.deployContract (self.contracts.account, data,
                                           self.contracts.registry.address)

    addr = [self.newSigningAddress () for _ in range (3)]
    for nm in ["foo", "bar", "baz"]:
      self.env.register ("p", nm)
    self.contracts.registry.functions\
        .setApprovalForAll (two.address, True)\
        .transact ({"from": self.contracts.account})
    self.generate (1)

    # Check that two moves with the same transaction ID will work fine
    # and result in different channel IDs.
    two.functions.setFlags (True, True).transact ()
    self.generate (1)
    txid = two.functions.send (
        "p", "foo", self.formatMove ({"c": {"addr": addr[0]}}),
        "p", "bar", self.formatMove ({"c": {"addr": addr[1]}})
    ).transact ()
    txid = uintToXaya (txid.hex ())
    self.generate (1)
    state = self.getGameState ()
    self.assertEqual (len (state["channels"]), 2)
    assert txid not in state["channels"]

    # For the rest of the test, we reuse the foo channel, and thus close (abort)
    # the one by bar.  For simplicity (so we don't have to figure out which
    # channel is who), just try closing both with both accounts, where only
    # the correct one will succeed.
    for c in self.getChannelIds ():
      self.sendMove ("bar", {"a": {"id": c}})
    self.generate (1)
    [channelId] = self.getChannelIds ()

    # Join one of the channels twice with different participants (and a reorg),
    # using our contract so that the tx/txid is actually the same between
    # the two joins.  But make sure the channel reinit is not.
    joinFcn = two.functions.send (
        "p", "bar", self.formatMove ({"j": {"id": channelId, "addr": addr[1]}}),
        "p", "baz", self.formatMove ({"j": {"id": channelId, "addr": addr[2]}})
    )
    beforeJoins = self.env.snapshot ()

    two.functions.setFlags (True, False).transact ()
    self.generate (1)
    joinTx = joinFcn.buildTransaction ({
      "from": self.contracts.account,
      "gas": 1_000_000,
    })
    txid1 = self.w3.eth.send_transaction (joinTx)
    self.generate (1)
    meta1 = self.getGameState ()["channels"][channelId]["meta"]
    self.assertEqual (meta1["participants"][1]["name"], "bar")

    beforeJoins.restore ()
    two.functions.setFlags (False, True).transact ()
    self.generate (1)
    txid2 = self.w3.eth.send_transaction (joinTx)
    self.assertEqual (txid1, txid2)
    self.generate (1)
    meta2 = self.getGameState ()["channels"][channelId]["meta"]
    self.assertEqual (meta2["participants"][1]["name"], "baz")

    assert meta1["reinit"] != meta2["reinit"]


if __name__ == "__main__":
  MultiMovesTest ().main ()
