# Copyright (C) 2019-2022 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
        xaya_rpc_url=self.xayanode.rpcurl)

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

    pending = self.rpc.xaya.name_pending ("p/" + name)
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
    Locks all UTXO's in the wallet, so that no name_update transactions
    can be made temporarily.  This does not affect signing messages.
    """

    outputs = self.rpc.xaya.listunspent ()
    self.rpc.xaya.lockunspent (False, outputs)

  def unlockFunds (self):
    """
    Unlocks all outputs in the wallet, so that name_update's can be done
    again successfully.
    """

    self.rpc.xaya.lockunspent (True)
