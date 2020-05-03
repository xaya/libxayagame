#!/usr/bin/env python3
# Copyright (C) 2019-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests how a game channel reacts to reorgs of important on-chain transactions
(channel creation, join, resolutions, winner statements).
"""

from shipstest import ShipsTest


class ReogTest (ShipsTest):

  def run (self):
    self.generate (110)

    # Create a test channel with two participants.  Remember the block hashes
    # where it was created and joined by the second one, so that we can
    # later invalidate those.
    self.mainLogger.info ("Creating test channel...")
    channelId = self.sendMove ("foo", {"c": {
      "addr": self.newSigningAddress (),
    }})
    self.generate (1)
    createBlk = self.rpc.xaya.getbestblockhash ()
    self.sendMove ("bar", {"j": {
      "id": channelId,
      "addr": self.newSigningAddress (),
    }})
    self.generate (1)
    joinBlk = self.rpc.xaya.getbestblockhash ()

    # Start up three channel daemons:  The two participants and
    # a third one, which will join the channel later in a reorged
    # alternate reality.
    self.mainLogger.info ("Starting channel daemons...")
    with self.runChannelDaemon (channelId, "foo") as foo, \
         self.runChannelDaemon (channelId, "bar") as bar, \
         self.runChannelDaemon (channelId, "baz") as baz:

      daemons = [foo, bar, baz]

      self.mainLogger.info ("Running initialisation sequence...")
      pos = """
        xxxx..xx
        ........
        xxx.xxx.
        ........
        xx.xx.xx
        ........
        ........
        ........
      """
      foo.rpc._notify.setposition (pos)
      bar.rpc._notify.setposition (pos)
      baz.rpc._notify.setposition (pos)

      # Play a couple of turns and save the resulting "original" state.
      for c in range (8):
        _, state = self.waitForPhase (daemons, ["shoot"])
        turn = state["current"]["state"]["whoseturn"]
        daemons[turn].rpc._notify.shoot (row=7, column=c)
      _, originalState = self.waitForPhase (daemons, ["shoot"])

      # Generate a couple of blocks to make sure this will be the longest
      # chain in the end.
      self.generate (10)

      self.mainLogger.info ("Reorg to channel creation...")
      self.rpc.xaya.invalidateblock (createBlk)
      self.assertEqual (foo.getCurrentState ()["existsonchain"], False)
      self.rpc.xaya.reconsiderblock (createBlk)
      self.rpc.xaya.invalidateblock (joinBlk)
      self.assertEqual (self.rpc.xaya.getbestblockhash (), createBlk)
      state = foo.getCurrentState ()
      self.assertEqual (state["existsonchain"], True)
      self.assertEqual (state["current"]["state"]["parsed"]["phase"],
                        "single participant")

      self.mainLogger.info ("Alternate join...")
      self.sendMove ("baz", {"j": {
        "id": channelId,
        "addr": self.newSigningAddress (),
      }})
      self.expectPendingMoves ("bar", [])
      self.expectPendingMoves ("baz", ["j"])
      self.generate (1)

      # Make sure it is baz' turn.  If not, miss a shot with foo.
      self.mainLogger.info ("Building alternate reality...")
      _, state = self.waitForPhase (daemons, ["shoot"])
      if state["current"]["state"]["whoseturn"] == 0:
        foo.rpc._notify.shoot (row=6, column=0)
        _, state = self.waitForPhase (daemons, ["shoot"])
      self.assertEqual (state["current"]["state"]["whoseturn"], 1)

      # Finish the game and let foo win.
      baz.rpc._notify.revealposition ()
      _, state = self.waitForPhase (daemons, ["finished"])
      self.assertEqual (state["current"]["state"]["parsed"]["winner"], 0)
      self.generate (1)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 1, "lost": 0},
          "baz": {"won": 0, "lost": 1},
        },
      })

      self.mainLogger.info ("Restoring original state...")
      self.rpc.xaya.reconsiderblock (joinBlk)
      state = foo.getCurrentState ()
      self.assertEqual (state["current"]["meta"],
                        originalState["current"]["meta"])
      self.assertEqual (state["current"]["state"]["parsed"],
                        originalState["current"]["state"]["parsed"])

      # Make sure it is foo's turn.
      _, state = self.waitForPhase (daemons, ["shoot"])
      if state["current"]["state"]["whoseturn"] == 1:
        bar.rpc._notify.shoot (row=6, column=1)
        _, state = self.waitForPhase (daemons, ["shoot"])
      self.assertEqual (state["current"]["state"]["whoseturn"], 0)

      # Test what happens if a resolution move gets reorged away.  Then the
      # player who resolved should still make sure it gets into the chain
      # again (since they obviously still know a better state).
      self.mainLogger.info ("Reorg of a resolution move...")
      bar.rpc.filedispute ()
      self.expectPendingMoves ("bar", ["d"])
      self.generate (1)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": False,
        "height": self.rpc.xaya.getblockcount (),
      })
      foo.rpc._notify.shoot (row=0, column=0)
      self.expectPendingMoves ("foo", ["r"])
      self.generate (1)
      resolutionBlk = self.rpc.xaya.getbestblockhash ()
      state = foo.getCurrentState ()
      assert "dispute" not in state

      self.rpc.xaya.invalidateblock (resolutionBlk)
      state = foo.getCurrentState ()
      self.assertEqual (state["dispute"], {
        "whoseturn": 0,
        "canresolve": True,
        "height": self.rpc.xaya.getblockcount (),
      })

      # A resolution is not resent if the previous transaction remained in
      # the mempool.  But in our case here, we actually resolved the dispute
      # and thus cleared the pending flag, and only then "reopened" it due
      # to the reorg.  For this case, at least the current implementation
      # sends a second resolution.  (To fix this, we would have to keep
      # track of previous resolutions even if their corresponding disputes
      # have been cleared already, which seems not worth the trouble for
      # the little extra potential benefit in some edge cases.)
      self.expectPendingMoves ("foo", ["r", "r"])
      self.generate (1)
      state = foo.getCurrentState ()
      assert "dispute" not in state

      self.mainLogger.info ("Letting the game end with a dispute...")
      bar.rpc.filedispute ()
      self.expectPendingMoves ("bar", ["d"])
      self.generate (11)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 0, "lost": 1},
          "bar": {"won": 1, "lost": 0},
        },
      })

      self.mainLogger.info ("Detaching a block and ending the game normally...")
      disputeTimeoutBlk = self.rpc.xaya.getbestblockhash ()
      self.rpc.xaya.invalidateblock (disputeTimeoutBlk)
      state = foo.getCurrentState ()
      self.assertEqual (state["existsonchain"], True)

      # We miss a shot with foo and reveal with bar, so that this time
      # foo wins the channel (but ordinarily).
      foo.rpc._notify.shoot (row=6, column=2)
      self.expectPendingMoves ("foo", ["r"])
      self.generate (1)
      self.waitForPhase (daemons, ["shoot"])
      bar.rpc._notify.revealposition ()
      _, state = self.waitForPhase (daemons, ["finished"])
      self.assertEqual (state["current"]["state"]["parsed"]["winner"], 0)
      txids = self.expectPendingMoves ("foo", ["w"])
      self.generate (1)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 1, "lost": 0},
          "bar": {"won": 0, "lost": 1},
        },
      })

      self.mainLogger.info ("Reorg and resending of winner statement...")
      winnerStmtBlk = self.rpc.xaya.getbestblockhash ()
      self.rpc.xaya.invalidateblock (winnerStmtBlk)
      state = foo.getCurrentState ()
      self.assertEqual (state["current"]["state"]["parsed"]["phase"],
                        "finished")
      self.assertEqual (state["current"]["state"]["parsed"]["winner"], 0)
      # The old transaction should have been restored to the mempool, and
      # we should not resend another one (but detect that it is still there
      # and just wait).
      newTxids = self.expectPendingMoves ("foo", ["w"])
      self.assertEqual (newTxids, txids)
      self.generate (1)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 1, "lost": 0},
          "bar": {"won": 0, "lost": 1},
        },
      })

      # Do a longer reorg, so that the original move will not be restored
      # to the mempool.  Verify that we send a new one.
      winnerStmtBlk = self.rpc.xaya.getbestblockhash ()
      self.generate (10)
      self.rpc.xaya.invalidateblock (winnerStmtBlk)
      state = foo.getCurrentState ()
      self.assertEqual (state["current"]["state"]["parsed"]["phase"],
                        "finished")
      self.assertEqual (state["current"]["state"]["parsed"]["winner"], 0)
      newTxids = self.expectPendingMoves ("foo", ["w"])
      assert txids != newTxids
      self.generate (1)
      self.expectGameState ({
        "channels": {},
        "gamestats": {
          "foo": {"won": 1, "lost": 0},
          "bar": {"won": 0, "lost": 1},
        },
      })


if __name__ == "__main__":
  ReogTest ().main ()
