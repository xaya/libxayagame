// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_CHANNELMANAGER_HPP
#define GAMECHANNEL_CHANNELMANAGER_HPP

#include "boardrules.hpp"
#include "broadcast.hpp"
#include "movesender.hpp"
#include "openchannel.hpp"
#include "rollingstate.hpp"
#include "signatures.hpp"

#include "proto/stateproof.pb.h"

#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace xaya
{

/* ************************************************************************** */

/**
 * The main logic for a channel daemon.  This class keeps track of the
 * state (except for game-specific pieces of data, of course), including
 * the actual board states known but also information about disputes.
 * It updates the states as moves and on-chain updates come in, provides
 * functions to query the state (used by the RPC server) and can request
 * resolutions if disputes are filed against the player and a newer state
 * is already known.
 *
 * This class does not do any locking or threading.  It awaits external
 * updates (through ProcessOnChain, ProcessOffChain or ProcessLocalMove),
 * and will in response update the state and trigger calls to the move
 * sender or offchain broadcast.
 */
class ChannelManager
{

public:

  class Callbacks;

private:

  /**
   * Data stored about a potential dispute on the current channel.
   */
  struct DisputeData
  {

    /** The block height at which the dispute is filed.  */
    unsigned height;

    /** The player whose turn it is at the dispute.  */
    int turn;

    /** The turn count at which the disputed state is.  */
    unsigned count;

    /**
     * The transaction ID of a sent resolution.  When there is no pending
     * resolution transaction, this is null.
     */
    uint256 pendingResolution;

    DisputeData ();

    DisputeData (const DisputeData&) = default;
    DisputeData& operator= (const DisputeData&) = default;

  };

  /** The board rules of the game being played.  */
  const BoardRules& rules;

  /** OpenChannel instance for this game.  */
  OpenChannel& game;

  /** Verification provider for signatures.  */
  const SignatureVerifier& verifier;
  /** Message signer for this user.  */
  SignatureSigner& signer;

  /** The ID of the managed channel.  */
  const uint256 channelId;

  /**
   * The Xaya name that corresponds to the player that is using the
   * current channel daemon (without p/ prefix).
   */
  const std::string playerName;

  /** Data about the board states we know.  */
  RollingState boardStates;

  /**
   * Broadcaster for off-chain moves.  This must be initialised before any
   * functions are called that would trigger a broadcast.
   */
  OffChainBroadcast* offChainSender = nullptr;

  /**
   * Instance for sending on-chain moves (disputes / resolutions).  This must
   * be set before any functions may be called that trigger such moves.
   */
  MoveSender* onChainSender = nullptr;

  /**
   * Version counter for the current state.  Whenever the state is changed,
   * this value is incremented.  It can be used to identify a certain state,
   * so that callers of WaitForChange can tell it which state they already
   * know and get notified on changes from that state.
   */
  int stateVersion = 1;

  /**
   * If set to false, it means that there is no on-chain data about the
   * channel ID.  This may be the case because the channel creation has not
   * been confirmed yet, or perhaps because the channel is already closed.
   */
  bool exists = false;

  /** The latest block hash for which we did an on-chain update.  */
  uint256 blockHash;
  /** The height of the latest on-chain update we know of.  */
  unsigned onChainHeight;

  /** Data about an open dispute, if any.  */
  std::unique_ptr<DisputeData> dispute;

  /**
   * The txid of a pending move putting the current state on chain.  Set to
   * null if there is none.  If multiple put-on-chain requests are sent,
   * this corresponds to the latest.
   */
  uint256 pendingPutStateOnChain;

  /**
   * The transaction ID of a dispute move we sent (if any).  Set to null
   * if there is none.
   */
  uint256 pendingDispute;

  /** Callbacks registered (e.g. for state updates).  */
  std::set<Callbacks*> callbacks;

  /**
   * Tries to apply a local move to the current state.  Returns true if
   * a change was made successfully.  This method just updates the
   * state, without triggering any more processing by itself.  It is
   * the shared code between ProcessLocalMove and processing of automoves.
   */
  bool ApplyLocalMove (const BoardMove& mv);

  /**
   * Tries to resolve the current dispute, if there is any.  This can be called
   * whenever a change may have happened that affects this, like a new state
   * being known (e.g. off-chain / local move) or an on-chain update.
   */
  void TryResolveDispute ();

  /**
   * Tries to apply a chain of automoves to the current state, if applicable.
   * Returns true if at least one move was found.
   */
  bool ProcessAutoMoves ();

  /**
   * Performs internal updates after the state was changed.  In particular,
   * this performs automoves, resolves disputes and notifies the OpenChannel
   * and WaitForChange listeners about a new change.
   *
   * If automoves were found or broadcast is true, then it also broadcasts
   * the new state to the off-chain channel.
   */
  void ProcessStateUpdate (bool broadcast);

  /**
   * Notifies threads waiting on cvStateChanged that a new state is available.
   * This also increments the state version.
   */
  void NotifyStateChange ();

  friend class ChannelManagerTestFixture;

public:

  explicit ChannelManager (const BoardRules& r, OpenChannel& oc,
                           const SignatureVerifier& v,
                           SignatureSigner& s,
                           const uint256& id, const std::string& name);

  ChannelManager () = delete;
  ChannelManager (const ChannelManager&) = delete;
  void operator= (const ChannelManager&) = delete;

  void SetOffChainBroadcast (OffChainBroadcast& s);
  void SetMoveSender (MoveSender& s);

  const uint256&
  GetChannelId () const
  {
    return channelId;
  }

  /**
   * Processes a (potentially) new move retrieved through the off-chain
   * broadcasting network.
   */
  void ProcessOffChain (const std::string& reinitId,
                        const proto::StateProof& proof);

  /**
   * Processes an on-chain update that did not contain any data for our channel.
   */
  void ProcessOnChainNonExistant (const uint256& blk, unsigned h);

  /**
   * Processes a (potentially) new on-chain state for the channel.
   */
  void ProcessOnChain (const uint256& blk, unsigned h,
                       const proto::ChannelMetadata& meta,
                       const BoardState& reinitState,
                       const proto::StateProof& proof,
                       unsigned disputeHeight);

  /**
   * Processes a move made locally, i.e. by the player who runs the channel
   * manager.  This tries to apply the move to the current state, sign the
   * resulting state, build a new state proof, and then broadcast it.
   */
  void ProcessLocalMove (const BoardMove& mv);

  /**
   * Tries to process auto moves if there are ones.  If moves can be found,
   * then they are broadcasted and other updates are done.  This function should
   * be called if some external change causes automoves to be potentially
   * available (e.g. some user input was made that was not directly a move
   * but affects the automoves logic).
   */
  void TriggerAutoMoves ();

  /**
   * Requests to send a resolution move with the current state, and returns
   * the txid if successful.  Resolutions for active disputes will be
   * sent automatically as needed, but this function can be used to
   * explicitly trigger one in situations where putting the current state
   * on-chain is useful for a different purpose.
   */
  uint256 PutStateOnChain ();

  /**
   * Requests to file a dispute with the current state.  Returns the txid
   * of the sent move (or null if sending failed).
   */
  uint256 FileDispute ();

  /**
   * Returns the current state version integer.
   */
  int
  GetStateVersion () const
  {
    return stateVersion;
  }

  /**
   * Returns the current board state if there is one, and null if no state
   * is known yet, e.g. because the channel does not yet exist on chain.
   *
   * The state is dynamic-cast to the given type, which must match the
   * actual runtime type.
   */
  template <typename State>
    const State* GetBoardState () const;

  /**
   * Returns the current state of this channel as JSON, suitable to be
   * sent to frontends.
   */
  Json::Value ToJson () const;

  /**
   * Register a callback instance to be invoked from this manager.
   */
  void RegisterCallback (Callbacks& cb);

  /**
   * Removes a registered callback.
   */
  void UnregisterCallback (Callbacks& cb);

};

/**
 * Interface for callbacks that can be invoked by a ChannelManager.
 */
class ChannelManager::Callbacks
{

public:

  Callbacks () = default;
  virtual ~Callbacks () = default;

  /**
   * Invoked when the current channel state changes.
   */
  virtual void
  StateChanged ()
  {}

};

/* ************************************************************************** */

/**
 * A ChannelManager reference together with a mutex, so that it can be accessed
 * from multiple threads (e.g. update event loops and an RPC server).
 * This class also supports a waitforchange-like interface for handling
 * state changes.
 */
class SynchronisedChannelManager : private ChannelManager::Callbacks
{

private:

  template <typename T>
    class Locked;

  /** The actual ChannelManager instance used.  */
  ChannelManager& cm;

  /**
   * Mutex for synchronising the internal channel manager.  Also used as
   * lock for the waitforchange condition variable.
   */
  mutable std::mutex mut;

  /**
   * Condition variable that gets signalled when the state is changed due
   * to on-chain updates, off-chain updates or local moves.  This is used
   * for waitforchange.
   */
  mutable std::condition_variable cvStateChanged;

  /**
   * If set to true, then future waitforchange calls will not block anymore.
   * We use this to ensure we can properly wake all waiters up when shutting
   * down, without them re-calling.
   */
  bool stopped = false;

  /**
   * Number of currently blocked waiter calls.
   */
  mutable unsigned waiting = 0;

  void StateChanged () override;

public:

  /**
   * Special value for the known version in WaitForChange that tells the
   * function to always block.
   */
  static constexpr int WAITFORCHANGE_ALWAYS_BLOCK = 0;

  explicit SynchronisedChannelManager (ChannelManager& c);
  ~SynchronisedChannelManager ();

  SynchronisedChannelManager () = delete;
  SynchronisedChannelManager (const SynchronisedChannelManager&) = delete;
  void operator= (const SynchronisedChannelManager&) = delete;

  /**
   * Returns a "locked instance" of the underlying ChannelManager.  This is
   * a movable instance that holds the underlying mutex while it exists
   * (like a std::unique_lock) and can be dereferenced to yield the
   * ChannelManager.
   *
   * This method returns a mutable instance of the ChannelManager.
   */
  Locked<ChannelManager> Access ();

  /**
   * Returns a read-only locked ChannelManager (similar to Access).
   */
  Locked<const ChannelManager> Read () const;

  /**
   * Disables processing of updates in the future.  This should be called
   * when shutting down the channel daemon.  It makes sure that all waiting
   * callers to WaitForChange are woken up, and no more callers will block
   * in the future.  Thus, this mechanism ensures that we can properly
   * shut down WaitForChange.
   *
   * This function must be called before a ChannelManager instance is
   * destructed.  Otherwise the destructor will CHECK-fail.
   */
  void StopUpdates ();

  /**
   * Blocks the calling thread until the state of the channel has (probably)
   * been changed.  This can be used by frontends to implement long-polling
   * RPC methods like waitforchange.  Note that the function may return
   * spuriously even if there is no new state.
   *
   * If the passed-in version is different from the current state version
   * already when starting the call, the function returns immediately.  Ideally,
   * clients should pass in the version they currently know (as returned
   * in the JSON state in "version"), so that we can avoid race conditions
   * when a change happens between two calls to WaitForChange.
   *
   * When WAITFORCHANGE_ALWAYS_BLOCK is passed as the known version, then the
   * function will always block until the next update.
   *
   * On return, the current (i.e. likely new) state is returned in the same
   * format as ToJson() would return.
   */
  Json::Value WaitForChange (int knownVersion) const;

};

/**
 * A "locked" ChannelManager instance.  While an object exists, it will
 * hold the mutex of an underlying SynchronisedChannelManager, and give
 * access to its ChannelManager.
 *
 * The type T is either "ChannelManager" or "const ChannelManager".
 */
template <typename T>
  class SynchronisedChannelManager::Locked
{

private:

  /** The underlying instance that can be accessed.  */
  T& instance;

  /** The lock on the mutex held by this instance.  */
  std::unique_lock<std::mutex> lock;

public:

  /**
   * Constructs a new instance, based on the underlying
   * SynchronisedChannelManager instance.
   */
  template <typename CM>
    explicit Locked (CM& underlying)
    : instance(underlying.cm), lock(underlying.mut)
  {}

  Locked (Locked&&) = default;
  Locked& operator= (Locked&&) = default;

  Locked (const Locked&) = delete;
  void operator= (const Locked&) = delete;

  T*
  operator-> ()
  {
    return &instance;
  }

  T&
  operator* ()
  {
    return instance;
  }

};

/* ************************************************************************** */

} // namespace xaya

#include "channelmanager.tpp"

#endif // GAMECHANNEL_CHANNELMANAGER_HPP
