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

} // namespace xaya

#endif // XAYAGAME_TESTUTILS_HPP
