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
two players in a channel, then any participant can close the channel
by declaring themselves loser.

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

## Detailed Specification

Let us now define the game rules (moves for the on-chain state, i.e.
management of channels, and the board rules for games in a channel)
in more detail.

### Board State and Rules

The *board state* is the state of a particular game of battleships in
a channel.  The exact sequence of moves in such a game is as follows:

1. Alice opens a channel by sending an on-chain move.
1. Bob joins the channel by sending also an on-chain move.
1. Alice sends two hashes as move in the channel, one of her chosen
   ship configuration and one of her chosen random seed.
1. Bob sends the hash of his ship configuration and at the same time
   his random seed in clear text.
1. Alice reveals her random seed.
1. The concatenation of both random seeds is hashed and the result
   used to derive a random bit.  That determines who (Alice or Bob)
   starts the main game and is the next player.  For this example, let us
   say Alice is chosen.
1. Alice guesses a coordinate on the board for her "shot".
  - Besides verifying that the coordinate is valid (i.e. in range, well-formed),
    it is also verified that the coordinate has not been guessed before.
    If it has, then the move is invalid.
1. Bob responds with "hit" or "miss".
  - If it is "hit", then it stays Alice's turn.
  - If it is a "miss", then Bob is the next player.
1. Any time, both Alice and Bob can decide to end the game instead of
   either guessing the next coordinate or responding to a guess.  Let's say
   that Bob ends the game.
1. In that situation, Bob reveals his ship configuration and salt.
  - The move is valid as long as this preimage matches his committed hash.
  - If any of Bob's previous answers to Alice's guesses was wrong according to
    the configuration, then the state is marked as "Alice won".
  - Similarly, if Bob's configuration is invalid for the game (because
    he placed too few ships, they touch or anything like that),
    then Alice also wins.
1. If Bob did not lie anywhere and all of Alice's ships have been hit
    (according to Alice's answers), then the state is marked as "Bob won".
1. Otherwise, Alice reveals her configuration in the next move.
  - As before, the move is valid if and only if the preimage matches Alice's
    committed hash.
  - If any of Alice's answers was wrong or her initial position invalid,
    then the state is marked as "Bob won".
1. If also Alice did not lie at any time, then Alice wins (since Bob did not
   sink all her ships).
1. No more valid moves can be sent on the channel.  At this point, the
   losing player will typically declare they lost with an on-chain move,
   which closes the channel.
1. If the loser does not do this, the winner can force-close the channel
   through a dispute (which the loser can only resolve by declaring their
   loss, as there are no more valid board moves they could send).

In a situation where both players have already sunk all ships, the rules
detailed above imply that the player who reveals the initial position first
wins the game (rather than e.g. the player who sunk all ships first).
However, when e.g. Alice hits the last of Bob's ships, it is her turn again
and she knows that Bob either lied or she sunk all ships.  Thus in that
situation, she can immediately end the game and is guaranteed to win
(unless she lied about her ships).  Hence the player who sinks all
enemy ships first can ensure they win the game.

### Game State and Moves

Xayaships uses the game ID `xs` for its on-chain GSP.  The global game state
consists of two types of data:

1. The statistics of won and lost games per Xaya name are stored in a simple
   SQLite database table.
1. Data about currently open game channels is stored through the game-channels
   framework (which has also its own table in the SQLite database).

Each **move** of the game must be a *JSON object*, containing *at most one*
of the following actions:

#### Creating a Channel

To create a new channel as the first participant, the following JSON value
can be used as move:

    {"c": {"addr": ADDRESS}}

Here, `ADDRESS` must be a string, and it will be set as signing address
for the player in the channel.  (It need not be a valid Xaya address, but
if it isn't, then obviously no messages can be signed on the channel
successfully.)

This will create a new channel, whose ID will be the transaction ID of
the move that created it.  There will be only one person in it initially,
waiting for a second player to join.

#### Joining a Channel

If a channel has only one participant, any (other) player may join it.
To do so, they should send a move of this form:

    {"j": {"id": CHANNEL-ID, "addr": ADDRESS}}

As with creating a channel, `ADDRESS` is the signing address the player
wishes to use within the channel.  `CHANNEL-ID` is the ID of the channel they
want to join, given as hex string.
Joining a channel is not possible if the channel already has two participants
or if the other participant is the same Xaya account (`p/` name).

After a second player joins a channel successfully, the on-channel game
begins properly.

#### Aborting a Channel

A channel that is open but has only one participant so far can be closed
any time by the one participant (e.g. if they waited for someone to join
but noone did).  This is done with a move of the form:

    {"a": {"id": CHANNEL-ID}}

Here, `CHANNEL-ID` is the channel's ID as hex string.  The move is only
valid if the channel has one participant and the name sending the move
is that one participant.
After processing this move, the channel will simply be closed (deleted from
the game state), without any changes to game stats of the player.

#### Declaring Loss

Either participant of an open channel can declare themselves the loser
any time:

    {"l": {"id": CHANNEL-ID, "r": REINIT}}

As before, `CHANNEL-ID` is the channel's ID as hex string, and `REINIT`
is the reinitialisation ID (raw data base64-encoded) for whose game state
the user lost.
(The latter prevents a loss declaration made on a channel from being
replayed in case there is a reorg that changes who joins the channel as
second player, and thus might in reality have a different outcome.)

Such a move is valid as long as the channel is currently open and has two
participants, of which the sending user is one.  There are no other checks
done (e.g. about the actual board state), as only the actual loser would
want to close a channel in this way in the first place.
If the move is valid, then the channel is closed (deleted from the game state),
and the statistics of both players are updated according to the declared
outcome.

This is how channels are typically closed in agreement after a game.
If the loser does not declare loss by themselves at the end of the game,
the winner can file a [resolution](#disputes) with the final state instead,
which will also close the channel.

#### <a name="disputes">Dispute Handling</a>

Disputes and resolutions can be processed by providing a
[state proof](https://github.com/xaya/libxayagame/blob/master/gamechannel/proto/stateproof.proto)
in a move.  To open a dispute in a channel, the move looks like this:

    {"d": {"id": CHANNEL-ID, "state": STATE-PROOF}}

Here, `CHANNEL-ID` is the channel's ID as hex string, and `STATE-PROOF` is
a base64-encoded, serialised `StateProof` message.  Resolutions have the
exact same format, except that the initial key is `r` instead of `d`.

Both disputes and resolutions can be filed by anyone, even non-participants
on the channel (although that will typically not be the case).  They are
valid as long as the channel has two participants, the state proof is valid
and the proven state is at least one turn further than the current state
known on-chain.

If the dispute or resolution is processed successfully, then the proven state
is recorded on-chain.  For a dispute, also the current block height is
stored.  For a resolution, any open dispute is marked as resolved.

If the on-chain state after a resolution corresponds to the end of the game
(i.e. the winner is determined already in the board state), then the
channel is automatically closed and the game resolved based on that outcome.
This can be used by the winner of a game to force-close the channel if the
loser does not declare their loss.

Note that it is possible to file a resolution without an open dispute,
in which case simply the on-chain board state of the channel is updated.

After processing each block, all channels with unresolved disputes that
have been opened **10 blocks before** will be force-closed.  For them, the
player whose turn it is according to the dispute's state loses.
