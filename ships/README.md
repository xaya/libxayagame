# Xayaships

*Xayaships* is an example game on the Xaya platform, demonstrating how
[**game
channels**](http://www.ledgerjournal.org/ojs/index.php/ledger/article/view/15)
can be used for trustless and fully decentralised, scalable off-chain gaming.

This is a simple implementation of the classical game
[Battleships](https://en.wikipedia.org/wiki/Battleship_%28game%29), where
players compete against each other and scores are recorded in the
on-chain game state for eternity.

Note that Xayaships is mainly intended to be an example game for demonstrating
the capabilities of game channels, testing the implementation and releasing
code that can be used and extended for other games.  Nevertheless, it provides
a fully-functional game, that is still fun to play.

The [game ID](https://github.com/xaya/xaya/blob/master/doc/xaya/games.md#games)
of Xayaships is `g/xs`.

## Game Overview

The on-chain game state of Xayaships keeps track, for each Xaya username,
of how many games that user has won and lost.  It also stores the
currently open game channels as needed to process disputes.

Channels can be opened at any time by sending a move requesting to
open a channel, and anyone can send a move to join an existing channel
waiting for a second player.  Channels can be closed anytime by the
player in it as long as no other has joined.  If there are already
two players in a channel, then it can be closed by any player (typically
the winner) providing a message stating who won and signed by both participants.

The battleships game itself (on a channel) is played on an **8x8 board**.
(This allows us conveniently to store a boolean map of the entire board
in an unsigned 64-bit integer.)
The following ships are available for placement by each player:

- 1x ship of size 4
- 2x ship of size 3
- 4x ship of size 2

Ships can be placed anywhere horizontally or vertically, but are not
allowed to overlap and also have to have a distance of at least one
empty grid square (including diagonally) to other ships.

At the beginning of the game, each player places their ships secretly
and publishes a *hash* of their chosen position (and some salt to make
brute-force guessing impossible).  Together with that hash, they also publish
a second hash of some random data.  After that, both reveal the second data,
and the combination is used to randomly determine who starts the game.

Then the players take turns guessing a location, and the other player
responds whether or not a ship has been hit; for the moment, this
report is "trusted".
When a ship has been hit, the player can make another guess.  Otherwise
it becomes the opponent's turn.

(Note that in contrast to typical rules, there is no distinction between
"hit" and "sunk".  This simplifies the game, which helps build a good
and easy to understand example game.)

At any time, a player can end the game by publishing their full configuration
and salt.  (This is typically done if all opponent ships have been sunk or
if the player gets an impossible answer from the opponent.)  In that case,
the opponent also has to publish their configuration
and salt, and they are checked against hashes as well as the reported outcomes
of shots during the game.  If a player lied, he loses the game.  Otherwise,
the ending player wins if and only if all opponent ships have been sunk.
