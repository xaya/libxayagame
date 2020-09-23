# Non-Fungible Assets

While Xaya empowers game developers to build *fully decentralised
and yet complex games*, there are also a lot of potential applications
based on just generic fungible and non-fungible tokens.  The `nonfungible` GSP
is an application built on Xaya that implements such tokens, which
everyone can issue, transfer and collect, in a way similar to
[ERC-1155](https://eips.ethereum.org/EIPS/eip-1155) on Ethereum.

Besides being a potentially useful application, this is also a simple
example for using libxayagame together with [SQLite](https://www.sqlite.org/),
and is used as a demo and for testing the
[Democrit framework](https://github.com/xaya/democrit) for atomic trading.

This application is using `g/nf` as its game ID.

## <a id="assets">Assets</a>

On the non-fungible platform, any user (i.e. `p/` name) can mint
*assets*.  Each asset is identified by two strings:  The minting user's name
and another string, which specifies the asset itself.  (But two users can
mint assets of the same name, and those two assets will be different.)
In the game state (as returned by the RPC interface) as well as moves,
an asset is identified by a JSON object of this form:

    {"m": MINTER, "a": ASSET}

Here, `MINTER` is the name of the minting user (without `p/` prefix) and
`ASSET` is the asset's name within that user's namespace.  `ASSET` must not
contain any unprintable characters (with code `<0x20`).

Each minted asset has a fixed supply, which is an unsigned 64-bit integer.
The supply is chosen by the minting user upon creation of the asset, and
will from then on be fixed.  No more of an already existing asset can
ever be created; only the existing pieces can be transferred or burned
(and thus decrease the existing supply).
If the supply is chosen as one token, this results in a true
non-fungible token.  If the supply is larger, then as many tokens are
minted that are between each other fungible (like a currency).

Assets may have a total supply of zero, either because they are minted like
that or because all issued tokens have been burned.  In that case, the asset
still continues to exist, and no other asset with the same name can ever
be minted by the same user again.

When an asset is created, the minting user can specify a custom string
that is associated to the asset.  This can for instance be a hash or
[IPFS link](https://ipfs.io/) to some metadata.  After minting, this
data is frozen and cannot ever be changed again.

The minting user's role is only special when issuing a new asset;
afterwards, they have no longer any more control over the token
than any other user on the network.  This guarantees that assets
issued through this application enjoy true ownership and trustlessness.

## Game State

The game state of this application has two main parts:  First, all assets
created (identified by the minting user's name and the asset's name), and
second the balances of each asset that each user holds.

For the first part, the game state simply contains an append-only list
of created assets and their associated custom string data.

The balances are simply unsigned 64-bit integers keyed by (user, asset)
holding the balance of a certain asset by some Xaya user (`p/` name).
Note that all balances are integers; but of course those balances could
be "interpreted" and shown by some front-end application as decimal numbers.

## Moves

Moves for `nonfungible` are in general represented either as a JSON object
(for individual operations) or an array of such JSON objects (for batched
operations in a single name-update transaction):

    {"g": {"nf": MOVE}}
    {"g": {"nf": [MOVE, MOVE, ...]}}

If multiple moves are sent as a batch, then they are evaluated in order
and independently.  In particular, later moves may depend on changes done
by earlier ones, and if a move in the sequence is invalid, it will be ignored
while other moves will still be executed (if valid).

Each `MOVE` can be one of three operations:

### Minting

A user can *mint a new asset*.  For this, the move specifies the new asset's
name, initial supply and custom data string:

    {"m": {"a": ASSET, "n": SUPPLY}}
    {"m": {"a": ASSET, "n": SUPPLY, "d": DATA}}

Here, `ASSET` and `DATA` are JSON strings, and `SUPPLY` is an unsigned
64-bit integer.  The `DATA` value is optional; if left out, then no
custom data will be set for the asset.  The move is invalid if an asset of the
given name exists already for the user doing the move.

The maximum possible value for `SUPPLY` is 2^60.  This is an arbitrary
choice; but it is large enough to be sufficient (a lot more than the
number of satoshis in Bitcoin) and small enough to not cause any complications
with implementations that perhaps do not support full unsigned 64-bit integers
easily.

Otherwise, a new asset with the given `DATA` string is added to the list
of assets, and `SUPPLY` units of it are initially given to the minting user's
balance (from where they can then be distributed as desired).

### <a id="transfers">Transfers</a>

Any user with a non-zero balance of some asset can transfer an arbitrary
number of units (up to the balance) to any other user:

    {"t": {"a": ASSET, "n": NUMBER, "r": RECIPIENT}}

Here, `ASSET` is a JSON object specifying the [asset to transfer](#assets).
`NUMBER` is an unsigned 64-bit integer, and `RECIPIENT` a JSON string
identifying the receiving user's name (without `p/` prefix).

If `NUMBER` is larger than zero and not larger than the user's current
balance of `ASSET`, then the balance is decreased by `NUMBER` and the
balance of `RECIPIENT` is increased accordingly.  Note that it is perfectly
valid to send a token to any arbitrary `RECIPIENT`, even if the corresponding
`p/` name has not even been registered on the Xaya blockchain yet, or would
be invalid as name on Xaya.  (In which
case anyone can register the name and thereby claim ownership of the token
balance sent to it.)

Note that if `NUMBER` is larger than the balance of the user, then
*no tokens* will be sent (not even a sufficiently smaller amount).
In other words, a transfer is executed as "all or nothing".

### Burns

Similarly, any user with a non-zero balance is free to burn one or more
units of a token.  They will simply be removed from the total supply:

    {"b": {"a": ASSET, "n": NUMBER}}

`ASSET` is again a JSON object identifying the [asset to burn](#assets).
`NUMBER` is a 64-bit integer and implies how many units will be burned.

If `NUMBER` is larger than zero and not larger than the user's current
balance of `ASSET`, then as many units will be removed from the user's
balance and consequently also from the total existing supply of the token.

As with [transfers](#transfers), no units are burned at all if `NUMBER`
is larger than the user's current balance.
