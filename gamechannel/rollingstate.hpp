// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_ROLLINGSTATE_HPP
#define GAMECHANNEL_ROLLINGSTATE_HPP

#include "boardrules.hpp"
#include "signatures.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayautil/uint256.hpp>

#include <deque>
#include <map>
#include <memory>
#include <string>

namespace xaya
{

/**
 * A helper class that keeps track of a queue of off-chain state updates
 * for their corresponding reinitialisations.  The total number of updates
 * kept is limited, to avoid DoS attacks that try to fill our memory
 * with bogus messages for invalid reinit IDs.
 *
 * When a new message comes in while the maximum size is already reached,
 * first other reinit's are removed (so that the "current" reinit, based
 * on how messages are received, is kept as much as possible).  If that is
 * not enough, the oldest messages for the current reinit are removed as well.
 *
 * This logic ensures that we will potentially keep the latest states (highest
 * turn count) for the current reinit in a situation where the peers are honest.
 * If a peer is trying to DoS us, there is nothing we can really do about it
 * anyway; in that case, the limit will ensure we do not run out of memory,
 * and in the unlikely case that we also received a valid off-chain message
 * before the corresponding reinit on-chain, the game will fall back to
 * a dispute and resolution in the worst case.
 */
class StateUpdateQueue
{

private:

  /** The maximum number of elements to keep.  */
  const size_t maxSize;

  /** Current number of elements in total (for all reinits).  */
  size_t size = 0;

  /** The queued updates for each reinit.  */
  std::map<std::string, std::deque<proto::StateProof>> updates;

public:

  explicit StateUpdateQueue (const size_t ms)
    : maxSize(ms)
  {}

  StateUpdateQueue () = delete;
  StateUpdateQueue (const StateUpdateQueue&) = delete;
  void operator= (const StateUpdateQueue&) = delete;

  /**
   * Inserts a new element (taking our limitations into account).
   */
  void Insert (const std::string& reinit, const proto::StateProof& upd);

  /**
   * Splices out all updates for the given reinit ID.  If there is a queue
   * for it, then it is removed internally and returned to the caller.
   * If there is not yet a queue, then an empty list is returned.
   */
  std::deque<proto::StateProof> ExtractQueue (const std::string& reinit);

  /**
   * Returns the current total size.
   */
  size_t
  GetTotalSize () const
  {
    return size;
  }

};

/**
 * All data about the current board state of a channel game.  This keeps track
 * of the latest known state including full proof for each reinitialisation
 * of the channel.  It is updated when new on-chain or off-chain data
 * is provided, and can return the current best state (proof) for use
 * in frontends or also e.g. for disputes and resolutions.
 *
 * We need to keep track of all known reinitialisations rather than only
 * the "current" one so that we can handle situations in which a move that
 * reinitialised the channel is rolled back.  Then we want to make sure that
 * we still have the "latest" state (and proof) for the resulting previous
 * reinitialisation as well.
 */
class RollingState
{

private:

  /**
   * The data corresponding to one reinitialisation.
   */
  struct ReinitData
  {

    /**
     * The metadata for this reinitialisation.  We keep a pointer to it rather
     * than the instance itself, because a reference to the proto is encoded
     * in latestState and we need it to remain valid even if the instance
     * gets moved around.
     */
    std::unique_ptr<proto::ChannelMetadata> meta;

    /** The initial state for that reinitialisation.  */
    BoardState reinitState;

    /** The turn count for the latest state known on chain.  */
    unsigned onChainTurn;

    /** The state proof for the latest state.  */
    proto::StateProof proof;

    /** The latest state as parsed object.  */
    std::unique_ptr<ParsedBoardState> latestState;

    ReinitData () = default;
    ReinitData (ReinitData&&) = default;
    ReinitData& operator= (ReinitData&&) = default;

    ReinitData (const ReinitData&) = delete;
    ReinitData& operator= (const ReinitData&) = delete;

  };

  /** Board rules to use for our game.  */
  const BoardRules& rules;

  /** Signature verifier for state proofs.  */
  const SignatureVerifier& verifier;

  /** The game ID of this application.  */
  const std::string& gameId;
  /** The ID of the channel this is for.  */
  const uint256& channelId;

  /**
   * All known data about reinitsialisations we have.  At the very beginning,
   * this map will be empty until the first block data is provided.  Until
   * this is done, GetLatestState and GetStateProof must not be called.
   */
  std::map<std::string, ReinitData> reinits;

  /**
   * For still unknown reinitialisations, we keep track of a list of
   * received off-chain updates.  We can't process them when we receive them
   * (as the reinit state is unknown), but we will process the full list once
   * the corresponding reinit gets created on chain.
   */
  StateUpdateQueue unknownReinitMoves;

  /** The reinit ID of the current reinitialisation.  */
  std::string reinitId;

public:

  explicit RollingState (const BoardRules& r, const SignatureVerifier& v,
                         const std::string& gId, const uint256& id);

  RollingState () = delete;
  RollingState (const RollingState&) = delete;
  void operator= (const RollingState&) = delete;

  /**
   * Returns the current latest state.
   */
  const ParsedBoardState& GetLatestState () const;

  /**
   * Returns a proof for the current latest state.
   */
  const proto::StateProof& GetStateProof () const;

  /**
   * Returns the turn count of the best state known on chain.
   */
  unsigned GetOnChainTurnCount () const;

  /**
   * Returns the reinitialisation ID of the channel for which the current
   * latest state (as returned by GetLatestState and GetStateProof) is.
   */
  const std::string& GetReinitId () const;

  /**
   * Returns the channel metadata corresponding to the currently best
   * reinitId.
   */
  const proto::ChannelMetadata& GetMetadata () const;

  /**
   * Updates the state for a newly received on-chain update.  This assumes
   * that the state proof is valid, and it also updates the "current"
   * reinitialisation to the one seen in the update.
   *
   * Returns true if an actual change has been made (i.e. the provided
   * state proof was valid and newer than what we had so far).
   */
  bool UpdateOnChain (const proto::ChannelMetadata& meta,
                      const BoardState& reinitState,
                      const proto::StateProof& proof);

  /**
   * Updates the state for a newly received off-chain state with the
   * given reinitialisation ID (if we know it).  This verifies the state proof,
   * and ignores invalid updates.
   *
   * Returns true if an actual change has been made, i.e. the reinit was
   * known and the state advanced forward with the new state proof.
   */
  bool UpdateWithMove (const std::string& updReinit,
                       const proto::StateProof& proof);

};

} // namespace xaya

#endif // GAMECHANNEL_ROLLINGSTATE_HPP
