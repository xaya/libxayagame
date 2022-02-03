# Game Channels

[Game
Channels](https://www.ledgerjournal.org/ojs/index.php/ledger/article/view/15)
are a technique for handling interactions between participants in a game
(or other application) in a very scalable, off-chain and almost real-time
fashion.

This folder contains a flexible implementation of the core framework
necessary to build applications that use them.  The starting point for
building a game-channel-enabled application is the [`BoardRules`
interface](https://github.com/xaya/libxayagame/blob/master/gamechannel/boardrules.hpp),
which defines the concrete rules by which interactions on a channel are done.

## Channel Core

The `channelcore` library contains the main parts of the general framework,
where the
[`ChannelManager`](https://github.com/xaya/libxayagame/blob/master/gamechannel/channelmanager.hpp)
is the main class that applications should use.  It handles all the core
tasks necessary for a channel application, like
constructing and verifying state proofs, handling disputes and resolutions, and
in general managing the state associated to a channel and handling the
actions necessary whenever input is received (on-chain, off-chain or from
a local frontend).

This library is relatively light-weight.  In particular, it does not
use any networking, JSON-RPC, threading or other complex dependencies.
As such, it can be used in contexts like web-based frontends (e.g. with wasm)
relatively easily, and also can be used to build channels that are not
necessarily linked to a Xaya GSP.

## Game-Channels on Xaya

More advanced tasks are implemented in the `gamechannel` library:  They
contain, for instance, event loops that automatically poll the state of
a channel from a Xaya GSP to update the local state, or provide the
tools necessary to build the on-chain parts for a channel-application easily
based on a Xaya GSP.
