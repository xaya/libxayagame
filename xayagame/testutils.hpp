// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_TESTUTILS_HPP
#define XAYAGAME_TESTUTILS_HPP

/* Shared utility functions for unit tests of xayagame.  */

#include "game.hpp"
#include "uint256.hpp"

#include <json/json.h>

#include <gtest/gtest.h>

#include <mutex>
#include <vector>
#include <string>

namespace xaya
{

/**
 * Returns a uint256 based on the given number, to be used as block hashes
 * in tests.
 */
uint256 BlockHash (unsigned num);

/**
 * Simple test fixture for tests that interact with a Game instance.  It is
 * declared friend of Game and makes some of the internal methods available
 * to the tests.
 */
class GameTestFixture : public testing::Test
{

private:

  /** Game ID to send to BlockAttach / BlockDetach.  */
  const std::string gameId;

protected:

  using State = Game::State;

  explicit GameTestFixture (const std::string& id)
    : gameId(id)
  {}

  static void
  SetUpTestCase ()
  {
    /* Use JSON-RPC V2 by the RPC client in Game.  It seems that V1 to V1
       does not work with jsonrpccpp for some reason.  */
    Game::rpcClientVersion = jsonrpc::JSONRPC_CLIENT_V2;
  }

  static std::string
  GetZmqEndpoint (const Game& g)
  {
    return g.zmq.addr;
  }

  static State
  GetState (const Game& g)
  {
    return g.state;
  }

  static void
  TrackGame (Game& g)
  {
    g.TrackGame ();
  }

  static void
  UntrackGame (Game& g)
  {
    g.UntrackGame ();
  }

  static void
  ReinitialiseState (Game& g)
  {
    std::lock_guard<std::mutex> lock(g.mut);
    g.ReinitialiseState ();
  }

  static void
  ForceState (Game& g, const State s)
  {
    std::lock_guard<std::mutex> lock(g.mut);
    g.state = s;
  }

  /**
   * Calls BlockAttach on the given game instance.  The function takes care
   * of setting up the blockData JSON object correctly based on the building
   * blocks given here.
   */
  void CallBlockAttach (Game& g, const std::string& reqToken,
                        const uint256& parentHash, const uint256& blockHash,
                        const Json::Value& moves, const bool seqMismatch) const;

  /**
   * Calls BlockDetach on the given game instance, setting up the blockData
   * object correctly.
   */
  void CallBlockDetach (Game& g, const std::string& reqToken,
                        const uint256& parentHash, const uint256& blockHash,
                        const Json::Value& moves, const bool seqMismatch) const;

};

/**
 * An extension to the GameTestFixture that keeps track of its own "blockchain"
 * in a stack of block hashes and associated move objects.  This can be
 * used to test basic situations of consistent attaches/detaches more easily
 * than with CallBlockAttach and CallBlockDetach.
 */
class GameTestWithBlockchain : public GameTestFixture
{

private:

  /** Stack of attached block hashes.  */
  std::vector<uint256> blockHashes;
  /**
   * Stack of corresponding move objects (the bottom-most entry in
   * blockHashes was set by SetStartingBlock and doesn't have any moves
   * associated to it).
   */
  std::vector<Json::Value> moveStack;

public:

  using GameTestFixture::GameTestFixture;

  /**
   * Resets the "blockchain" to have the given starting block (need not be
   * the real genesis block, it is just the block from where the next attach
   * will be done).
   */
  void SetStartingBlock (const uint256& hash);

  /**
   * Attaches the given next block on top of the current blockchain stack.
   * This calls BlockAttach on the game instance with no request token (empty
   * string) and without sequence mismatch.
   */
  void AttachBlock (Game& g, const uint256& hash, const Json::Value& moves);

  /**
   * Detaches the current top block from the simulated blockchain.
   */
  void DetachBlock (Game& g);

};

} // namespace xaya

#endif // XAYAGAME_TESTUTILS_HPP
