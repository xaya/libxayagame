# Xaya Mover

**Mover** is a simple game on the [Xaya platform](https://xaya.io/), where
players can **move around in an infinite plane**.  It is fully functional, but
mainly meant to illustrate the use of `libxayagame`.  The source code can
also be used as basis for implementing more complex games.
The Xaya game ID for mover is **`mv`**.

## Game State

The game state of Mover holds a list of players that have entered the world.
Each of those players is at **some integer coordinate on an infinite 2D plane**.
Optionally, the player may be **moving for `n` more turns** into one of the
**eight cardinal or diagonal directions**.

## Moves

[Moves](https://github.com/xaya/Specs/blob/master/games.md#moves)
are represented by a JSON object that specifies how the player
should be moved in the game world:

    {
      "d": DIRECTION,
      "n": STEPS
    }

Here, **`DIRECTION`** specifies the direction in which the player should
be moved.  It can be one of the following (inspired by Vim movement keys
and [Nethack](https://nethack.org/)):

Value | Direction      | Vector
----- | -------------- | ----------
`l`   | right          | `(1, 0)`
`h`   | left           | `(-1, 0)`
`k`   | up             | `(0, 1)`
`j`   | down           | `(0, -1)`
`u`   | right and up   | `(1, 1)`
`n`   | right and down | `(1, -1)`
`y`   | left and up    | `(-1, 1)`
`b`   | left and down  | `(-1, -1)`

**`STEPS`** is a positive integer that specifies for how many turns the player
should move into the specified direction.
The value may be at most 1,000,000.

## Updating the Game State

For updating the game state with a new block, the following steps are performed:

1. Players that sent a move in the block but are not yet on the map are added
   at the **initial position `(0, 0)`**.
2. All players that sent a move have their direction and number of steps
   **set to what was specified in the move**.
3. All players with a non-zero number of movement steps left are **moved one
   step** in their respective directions.
4. The number of movement steps left for each player is **decremented by one**
   (until it is zero).
5. For all players whose steps left is (now) zero, the **movement direction
   is cleared**.
