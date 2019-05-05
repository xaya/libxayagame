// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamelogic.hpp"

#include "hash.hpp"

#include <glog/logging.h>

namespace xaya
{

/* ************************************************************************** */

std::string
ChainToString (const Chain c)
{
  switch (c)
    {
    case Chain::UNKNOWN:
      return "unknown";
    case Chain::MAIN:
      return "main";
    case Chain::TEST:
      return "test";
    case Chain::REGTEST:
      return "regtest";
    }

  LOG (FATAL) << "Invalid chain enum value: " << static_cast<int> (c);
}

/* ************************************************************************** */

Context::Context (const GameLogic& l, const uint256& rndSeed)
  : logic(l)
{
  rnd.Seed (rndSeed);
}

Chain
Context::GetChain () const
{
  CHECK (logic.chain != Chain::UNKNOWN);
  return logic.chain;
}

const std::string&
Context::GetGameId () const
{
  CHECK (!logic.gameId.empty ());
  return logic.gameId;
}

/* ************************************************************************** */

/**
 * Helper class that controls setting and unsetting the context pointer
 * in a GameLogic instance through RAII.
 */
class GameLogic::ContextSetter
{

private:

  /** The underlying GameLogic instance.  */
  GameLogic& logic;

  /** The Context instance we point to.  */
  Context& ctx;

public:

  /**
   * Constructs the instance and sets the context pointer.
   */
  explicit ContextSetter (GameLogic& l, Context& c)
    : logic(l), ctx(c)
  {
    CHECK (logic.ctx == nullptr);
    logic.ctx = &ctx;
  }

  /**
   * Destructs the instance, unsetting the pointer.
   */
  ~ContextSetter ()
  {
    CHECK (logic.ctx == &ctx);
    logic.ctx = nullptr;
  }

  ContextSetter (const ContextSetter&) = delete;
  void operator= (const ContextSetter&) = delete;

};

void
GameLogic::InitialiseGameContext (const Chain c, const std::string& id)
{
  CHECK (c != Chain::UNKNOWN);
  CHECK (!id.empty ());

  CHECK (chain == Chain::UNKNOWN) << "GameLogic is already initialised";
  chain = c;
  gameId = id;
}

Chain
GameLogic::GetChain () const
{
  CHECK (chain != Chain::UNKNOWN);
  return chain;
}

Context&
GameLogic::GetContext ()
{
  CHECK (ctx != nullptr) << "No Internal callback is running at the moment";
  return *ctx;
}

const Context&
GameLogic::GetContext () const
{
  CHECK (ctx != nullptr) << "No Internal callback is running at the moment";
  return *ctx;
}

GameStateData
GameLogic::GetInitialState (unsigned& height, std::string& hashHex)
{
  CHECK (!gameId.empty ());

  SHA256 rndSeed;
  rndSeed << "initial state" << gameId;

  Context context(*this, rndSeed.Finalise ());
  ContextSetter setter(*this, context);

  return GetInitialStateInternal (height, hashHex);
}

namespace
{

/**
 * Returns the RNG seed for block attaches / detaches.
 */
uint256
BlockRngSeed (const std::string& gameId, const Json::Value& blockData)
{
  CHECK (!gameId.empty ());

  const auto& blk = blockData["block"];
  CHECK (blk.isObject ());
  const auto& coreSeedVal = blk["rngseed"];
  CHECK (coreSeedVal.isString ());

  uint256 coreSeed;
  CHECK (coreSeed.FromHex (coreSeedVal.asString ()));

  SHA256 rndSeed;
  rndSeed << "block" << gameId << coreSeed;

  return rndSeed.Finalise ();
}

} // anonymous namespace

GameStateData
GameLogic::ProcessForward (const GameStateData& oldState,
                           const Json::Value& blockData,
                           UndoData& undoData)
{
  Context context(*this, BlockRngSeed (gameId, blockData));
  ContextSetter setter(*this, context);

  return ProcessForwardInternal (oldState, blockData, undoData);
}

GameStateData
GameLogic::ProcessBackwards (const GameStateData& newState,
                             const Json::Value& blockData,
                             const UndoData& undoData)
{
  Context context(*this, BlockRngSeed (gameId, blockData));
  ContextSetter setter(*this, context);

  return ProcessBackwardsInternal (newState, blockData, undoData);
}

Json::Value
GameLogic::GameStateToJson (const GameStateData& state)
{
  return state;
}

/* ************************************************************************** */

GameStateData
CachingGame::ProcessForwardInternal (const GameStateData& oldState,
                                     const Json::Value& blockData,
                                     UndoData& undoData)
{
  const GameStateData newState = UpdateState (oldState, blockData);
  undoData = UndoData (oldState);
  return newState;
}

GameStateData
CachingGame::ProcessBackwardsInternal (const GameStateData& newState,
                                       const Json::Value& blockData,
                                       const UndoData& undoData)
{
  return GameStateData (undoData);
}

/* ************************************************************************** */

} // namespace xaya
