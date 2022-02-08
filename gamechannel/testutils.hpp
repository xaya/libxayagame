// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_TESTUTILS_HPP
#define GAMECHANNEL_TESTUTILS_HPP

#include "broadcast.hpp"
#include "movesender.hpp"
#include "signatures.hpp"

#include "proto/metadata.pb.h"

#include <xayautil/uint256.hpp>

#include <gmock/gmock.h>

#include <queue>
#include <string>
#include <vector>

namespace xaya
{

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
  void ExpectOne (const std::string& gameId, const uint256& channelId,
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
 * Mock instance for a basic off-chain broadcast.
 */
class MockOffChainBroadcast : public OffChainBroadcast
{

public:

  MockOffChainBroadcast (const uint256& i)
    : OffChainBroadcast(i)
  {
    /* Expect no calls by default.  */
    EXPECT_CALL (*this, SendMessage (testing::_)).Times (0);
  }

  MOCK_METHOD1 (SendMessage, void (const std::string& msg));

};

} // namespace xaya

#endif // GAMECHANNEL_TESTUTILS_HPP
