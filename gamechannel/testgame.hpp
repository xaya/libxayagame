// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_TESTGAME_HPP
#define GAMECHANNEL_TESTGAME_HPP

#include "boardrules.hpp"
#include "channelgame.hpp"
#include "movesender.hpp"
#include "openchannel.hpp"
#include "signatures.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <xayagame/sqlitestorage.hpp>
#include <xayagame/testutils.hpp>
#include <xayautil/uint256.hpp>

#include <json/json.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <queue>
#include <string>

namespace xaya
{

class TestGameFixture;

/**
 * Board rules for a trivial example game used in unit tests.  The game goes
 * like this:
 *
 * The current state is a pair of numbers, encoded simply in a string.  Those
 * numbers are a "current number" and the turn count.  The current
 * turn is for player (number % 2).  When the number is 100 or above, then
 * the game is finished.  A move is simply another, strictly positive number
 * encoded as a string, which gets added to the current "state number".
 * The turn count is simply incremented on each turn made.
 */
class AdditionRules : public BoardRules
{

public:

  std::unique_ptr<ParsedBoardState> ParseState (
      const uint256& channelId, const proto::ChannelMetadata& meta,
      const BoardState& s) const override;

  ChannelProtoVersion GetProtoVersion (
      const proto::ChannelMetadata& meta) const override;

};

/**
 * OpenChannel implementation for our test game.
 */
class AdditionChannel : public OpenChannel
{

private:

  /** If set, then automoves will be processed.  */
  bool automovesEnabled = true;

public:

  Json::Value ResolutionMove (const uint256& channelId,
                              const proto::StateProof& proof) const override;

  Json::Value DisputeMove (const uint256& channelId,
                           const proto::StateProof& proof) const override;

  /**
   * When the last digit of the current number in the addition game is 6-9,
   * we apply an automove of +2.  This way, we can test both a situation
   * where just one automove is applied (8 -> 10) and one where
   * two moves in a row are automatic (6 -> 8 -> 10).
   */
  bool MaybeAutoMove (const ParsedBoardState& state, BoardMove& mv) override;

  /**
   * If the state reached exactly 100, then we send an on-chain move (that
   * is just a string "100").  This can be triggered through auto-moves as well.
   */
  void MaybeOnChainMove (const ParsedBoardState& state,
                         MoveSender& sender) override;

  /**
   * Enables or disables processing of automoves.  When they are disabled,
   * then MaybeAutoMove will always return false, independent of the current
   * state.  This can be used to simulate situations in real games where
   * automoves may become possible for some situation only after user input
   * of some data (but not the move itself).
   */
  void
  SetAutomovesEnabled (const bool val)
  {
    automovesEnabled = val;
  }

};

/**
 * Subclass of ChannelGame that implements a trivial game only as much as
 * necessary for unit tests of the game-channel framework.
 */
class TestGame : public ChannelGame
{

private:

  /** The mock verifier used.  */
  const SignatureVerifier& mockVerifier;

protected:

  void SetupSchema (SQLiteDatabase& db) override;
  void GetInitialStateBlock (unsigned& height,
                             std::string& hashHex) const override;
  void InitialiseState (SQLiteDatabase& db) override;
  void UpdateState (SQLiteDatabase& db, const Json::Value& blockData) override;
  Json::Value GetStateAsJson (const SQLiteDatabase& db) override;

  const SignatureVerifier& GetSignatureVerifier () override;
  const BoardRules& GetBoardRules () const override;

  friend class TestGameFixture;

public:

  explicit TestGame (const SignatureVerifier& v)
    : mockVerifier(v)
  {}

  AdditionRules rules;
  AdditionChannel channel;

  using ChannelGame::ProcessDispute;
  using ChannelGame::ProcessResolution;

};

/**
 * Mock signature verifier.
 */
class MockSignatureVerifier : public SignatureVerifier
{

public:

  MOCK_METHOD (std::string, RecoverSigner,
               (const std::string&, const std::string&), (const, override));

  /**
   * Sets up the mock to validate *any* message with the given
   * signature as belonging to the given address.
   */
  void SetValid (const std::string& sgn, const std::string& addr);

  /**
   * Expects exactly one call to verification with the given message
   * and signature (both as binary).  Returns a valid response for the
   * given address.
   */
  void ExpectOne (const uint256& channelId,
                  const proto::ChannelMetadata& meta,
                  const std::string& topic,
                  const std::string& msg, const std::string& sgn,
                  const std::string& addr);

};

/**
 * Mock signature signer.
 */
class MockSignatureSigner : public SignatureSigner
{

private:

  /** The address returned from GetAddress.  */
  std::string address;

public:

  /**
   * Sets the address this signer should consider itself for.
   */
  void
  SetAddress (const std::string& addr)
  {
    address = addr;
  }

  std::string
  GetAddress () const override
  {
    return address;
  }

  MOCK_METHOD (std::string, SignMessage, (const std::string&), (override));

};

/**
 * Fake instance for TransactionSender for testing.
 */
class MockTransactionSender : public TransactionSender
{

private:

  /** The current simulated "mempool".  */
  std::set<uint256> mempool;

  /** The queue of txid's to be returned.  */
  std::queue<uint256> txidQueue;

  /** Counter used to generate unique txid's.  */
  unsigned cnt = 0;

public:

  MockTransactionSender ();

  /**
   * Marks the mock for expecting a call with a raw string value that satisfies
   * the given mater.  The call will throw an error.
   */
  void ExpectFailure (const std::string& name,
                      const testing::Matcher<const std::string&>& m);

  /**
   * Marks the mock for expecting n calls where the passed-in string value
   * satisfies a gMock matcher.  It will return a list of n unique txids
   * (generated automatically), which the move calls will return and which will
   * also be marked as pending until ClearMempool is called the next time.
   */
  std::vector<uint256> ExpectSuccess (
      unsigned n, const std::string& name,
      const testing::Matcher<const std::string&>& m);

  /**
   * Expects exactly one successful call.
   */
  uint256 ExpectSuccess (const std::string& name,
                         const testing::Matcher<const std::string&>& m);

  /**
   * Clears the internal mempool, simulating a block being mined.
   */
  void ClearMempool ();

  MOCK_METHOD (uint256, SendRawMove,
               (const std::string&, const std::string&), (override));
  bool IsPending (const uint256& txid) const override;

};

/**
 * Test fixture that constructs a TestGame instance with an in-memory database
 * and exposes that to the test itself.  It also runs a mock Xaya Core server
 * for use together with signature verification.
 */
class TestGameFixture : public testing::Test
{

protected:

  MockSignatureVerifier verifier;
  MockSignatureSigner signer;

  TestGame game;

  /**
   * Initialises the test case.  This connects the game instance to an
   * in-memory database and sets up the schema on it.
   */
  TestGameFixture ();

  /**
   * Returns the raw database handle of the test game.
   */
  SQLiteDatabase& GetDb ();

};

} // namespace xaya

#endif // GAMECHANNEL_TESTGAME_HPP
