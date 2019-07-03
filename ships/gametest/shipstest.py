# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from xayagametest.testcase import XayaGameTest

import os
import os.path

from gamechannel import signatures
from gamechannel.proto import stateproof_pb2

from proto import boardstate_pb2
from proto import winnerstatement_pb2

from google.protobuf import text_format

import base64


class ShipsTest (XayaGameTest):
  """
  An integration test for the ships on-chain GSP.
  """

  def __init__ (self):
    top_builddir = os.getenv ("top_builddir")
    if top_builddir is None:
      top_builddir = "../.."
    shipsd = os.path.join (top_builddir, "ships", "shipsd")
    super (ShipsTest, self).__init__ ("xs", shipsd)

  def openChannel (self, names, addresses):
    """
    Creates a channel and joins it, so that we end up with a fully set-up
    channel and two participants.  names and addresses should be arrays of
    length two each, giving the names / addresses for the participants.
    Returns the ID of the created channel.
    """

    self.assertEqual (len (names), 2)
    self.assertEqual (len (addresses), 2)
    cid = self.sendMove (names[0], {"c": {"addr": addresses[0]}})
    self.generate (1)
    self.sendMove (names[1], {"j": {"id": cid, "addr": addresses[1]}})
    self.generate (1)

    state = self.getGameState ()
    assert cid in state["channels"]

    return cid

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
    signedData = signatures.createForChannel (self.rpc.xaya, channel,
                                              "state", stateBytes)
    res.initial_state.CopyFrom (signedData)

    return base64.b64encode (res.SerializeToString ())

  def getWinnerStatement (self, cid, winner):
    """
    Constructs a winner statement for the given winner index and signs it
    by all players whose addresses we have.  The returned statement
    is already serialised and base64-encoded, so it can be put into a move.
    """

    state = self.getGameState ()
    channel = state["channels"][cid]

    stmt = winnerstatement_pb2.WinnerStatement ()
    stmt.winner = winner

    data = stmt.SerializeToString ()
    sgn = signatures.createForChannel (self.rpc.xaya, channel,
                                       "winnerstatement", data)

    return base64.b64encode (sgn.SerializeToString ())

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
