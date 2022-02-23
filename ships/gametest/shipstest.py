# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from contextlib import contextmanager
import json
import os
import os.path
import time

from gamechannel import channeltest
from gamechannel import signatures
from gamechannel.proto import stateproof_pb2

from proto import boardstate_pb2

from google.protobuf import text_format

import base64


GAME_ID = "xs"


class ShipsTest (channeltest.TestCase):
  """
  An integration test for the ships on-chain GSP and (potentially)
  the channel daemon in addition.
  """

  def __init__ (self):
    top_builddir = os.getenv ("top_builddir")
    if top_builddir is None:
      top_builddir = "../.."
    shipsd = os.path.join (top_builddir, "ships", "shipsd")
    channeld = os.path.join (top_builddir, "ships", "ships-channel")
    super (ShipsTest, self).__init__ (GAME_ID, shipsd, channeld)

  @contextmanager
  def runBaseChainEnvironment (self):
    with self.runXayaXEthEnvironment () as env:
      # In order to support locking and unlocking of the ability to send
      # moves, we use another account (not the owner) that is approved
      # on the default address' names.
      self.fromAddr = self.w3.eth.accounts[1]
      assert self.fromAddr != self.contracts.account

      self.contracts.registry.functions.setApprovalForAll (self.fromAddr, True)\
          .transact ({"from": self.contracts.account})
      env.generate (1)

      yield env

  def getChannelIds (self):
    """
    Returns a set of all channel IDs that are currently open.
    """

    return set (self.getGameState ()["channels"].keys ())

  def openChannel (self, names, addresses=None):
    """
    Creates a channel and joins it, so that we end up with a fully set-up
    channel and two participants.  names hould be an array of
    length two, giving the names for the participants.
    Returns the ID of the created channel and their signing addresses.
    """

    addresses = [self.newSigningAddress () for _ in range (2)]

    self.assertEqual (len (names), 2)
    self.assertEqual (len (addresses), 2)
    self.sendMove (names[0], {"c": {"addr": addresses[0]}})

    # Take a snapshot of channels before and after the creation, to
    # determine which the new channel's ID is (so that the code is flexible
    # and does not assume it is necessarily the txid).  This assumes that
    # none of the other channels is closed right in that block, but hopefully
    # that's not happening for our tests.
    idsBefore = self.getChannelIds ()
    self.generate (1)
    idsAfter = self.getChannelIds ()
    [cid] = idsAfter - idsBefore

    self.sendMove (names[1], {"j": {"id": cid, "addr": addresses[1]}})
    self.generate (1)

    state = self.getGameState ()
    assert cid in state["channels"]

    return cid, addresses

  def runChannelDaemon (self, channelId, playerName, address):
    """
    Runs a channel daemon with the arguments set for the
    ships-channel binary.
    """

    # Look up the private key associated to the requested signing address.
    assert address in self.signerAccounts, \
        "%s is not a signing address" % address
    privkey = self.signerAccounts[address].key.hex ()

    return super ().runChannelDaemon (playerName,
        channelid=channelId,
        privkey=privkey,
        eth_rpc_url=self.ethnode.rpcurl,
        from_address=self.fromAddr,
        accounts_contract=self.contracts.registry.address)

  def getStateProof (self, cid, stateStr):
    """
    Constructs a base64-encoded, serialised StateProof for the board state
    given as text proto.  This is then ready to send in a dispute or resolution
    move for the test.
    """

    state = self.getGameState ()
    channel = state["channels"][cid]

    state = boardstate_pb2.BoardState ()
    text_format.Parse (stateStr, state)
    stateBytes = state.SerializeToString ()

    res = stateproof_pb2.StateProof ()
    signedData = signatures.createForChannel (self.getSigningProvider (),
                                              GAME_ID, channel,
                                              "state", stateBytes)
    res.initial_state.CopyFrom (signedData)

    return base64.b64encode (res.SerializeToString ()).decode ("ascii")

  def expectChannelState (self, cid, phase, disputeHeight):
    """
    Extracts the state of our test channel and verifies that it matches
    the given phase and disputeHeight; the latter might be None if there
    should not be any dispute.
    """

    state = self.getGameState ()
    channel = state["channels"][cid]

    if disputeHeight is None:
      assert "disputeheight" not in channel
    else:
      self.assertEqual (channel["disputeheight"], disputeHeight)

    self.assertEqual (channel["state"]["parsed"]["phase"], phase)

  def expectPendingMoves (self, name, types):
    """
    Expects that the given player has exactly a certain number
    and type (as per the top-level move value field) of moves
    pending in the mempool.

    Returns the txids of the pending moves.
    """

    self.log.info ("Expecting pending moves for %s: %s" % (name, types))

    # Make sure to wait a bit before requesting the pending moves,
    # to avoid tests being flaky.
    time.sleep (1)

    pending = self.namePending ("p/" + name)
    actualTypes = []
    for p in pending:
      val = json.loads (p["value"])
      if ("g" not in val) or ("xs" not in val["g"]):
        continue
      mvKeys = list (val["g"]["xs"].keys ())
      self.assertEqual (len (mvKeys), 1)
      actualTypes.append (mvKeys[0])

    self.assertEqual (actualTypes, types)

    return [p["txid"] for p in pending]

  def waitForPhase (self, daemons, phases):
    """
    Waits for the channel daemons to be in one of the passed in phases.
    Returns the actual phase reached and the corresponding state.
    """

    self.log.info ("Waiting for phase in: %s" % phases)
    while True:
      state = self.getSyncedChannelState (daemons)
      phase = state["current"]["state"]["parsed"]["phase"]
      if phase in phases:
        self.log.info ("Reached phase %s" % phase)
        return phase, state

      self.log.warning ("Phase is %s, waiting to catch up..." % phase)
      time.sleep (0.01)

  def lockFunds (self):
    """
    Disapproves the "from" address from the test names.  This will result
    in channel-daemon-sent transactions to fail.
    """

    self.contracts.registry.functions.setApprovalForAll (self.fromAddr, False)\
        .transact ({"from": self.contracts.account})
    self.generate (1)

  def unlockFunds (self, name=None):
    """
    Re-approves the "from" address used by channel daemons to operate
    on the test names.

    If an explicit name is given, then we only give explicit approval
    for that one name.
    """

    if name is None:
      self.contracts.registry.functions.setApprovalForAll (self.fromAddr, True)\
          .transact ({"from": self.contracts.account})
    else:
      tokenId = self.contracts.registry.functions.tokenIdForName ("p", name)\
          .call ()
      self.contracts.registry.functions.approve (self.fromAddr, tokenId)\
          .transact ({"from": self.contracts.account})

    self.generate (1)

  def namePending (self, nm):
    """
    Returns the pending move transactions for the given name from the mempool.
    """

    res = []

    allTx = self.w3.geth.txpool.content ()["pending"]
    for perAddress in allTx.values ():
      for tx in perAddress.values ():
        if tx["to"].lower () != self.contracts.registry.address.lower ():
          continue
        fcn, args = self.contracts.registry.decode_function_input (tx["input"])
        if fcn.function_identifier != "move":
          continue
        if "%s/%s" % (args["ns"], args["name"]) != nm:
          continue
        res.append ({
          "value": args["mv"],
          "txid": tx["hash"][2:],
        })

    return res
