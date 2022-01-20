// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testgame.hpp"

#include "movesender.hpp"
#include "protoutils.hpp"
#include "signatures.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <gmock/gmock.h>

#include <glog/logging.h>

#include <sstream>

namespace xaya
{

/* ************************************************************************** */

namespace
{

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::Throw;

struct ParsedState
{
  int number;
  int count;
};

bool
ParsePair (const std::string& s, ParsedState& res)
{
  std::istringstream in(s);
  in >> res.number >> res.count;

  if (!in)
    {
      LOG (WARNING) << "Invalid game state: " << s;
      return false;
    }

  return true;
}

class AdditionState : public ParsedBoardState
{

private:

  const ParsedState data;

public:

  explicit AdditionState (const BoardRules& r, const uint256& id,
                          const proto::ChannelMetadata& m,
                          const ParsedState& d)
    : ParsedBoardState(r, id, m), data(d)
  {}

  AdditionState () = delete;
  AdditionState (const AdditionState&) = delete;
  void operator= (const AdditionState&) = delete;

  bool
  Equals (const BoardState& other) const override
  {
    ParsedState p;
    if (!ParsePair (other, p))
      return false;

    return p.number == data.number && p.count == data.count;
  }

  int
  WhoseTurn () const override
  {
    if (data.number >= 100)
      return ParsedBoardState::NO_TURN;

    return data.number % 2;
  }

  unsigned
  TurnCount () const override
  {
    return data.count;
  }

  bool
  ApplyMove (const BoardMove& mv, BoardState& newState) const override
  {
    /* The game-channel engine should never invoke ApplyMove on a 'no turn'
       situation.  Make sure to verify that.  */
    CHECK (WhoseTurn () != ParsedBoardState::NO_TURN);

    std::istringstream mvIn(mv);
    int add;
    mvIn >> add;
    if (add <= 0)
      return false;

    std::ostringstream out;
    out << (data.number + add) << " " << (data.count + 1);
    newState = out.str ();

    return true;
  }

  Json::Value
  ToJson () const override
  {
    Json::Value res(Json::objectValue);
    res["number"] = data.number;
    res["count"] = data.count;
    return res;
  }

  bool
  MaybeAutoMove (BoardMove& mv) const
  {
    if (data.number % 10 < 6)
      return false;

    mv = "2";
    return true;
  }

  void
  MaybeOnChainMove (MoveSender& sender) const
  {
    if (data.number == 100)
      sender.SendMove (Json::Value ("100"));
  }

};

} // anonymous namespace

std::unique_ptr<ParsedBoardState>
AdditionRules::ParseState (const uint256& channelId,
                           const proto::ChannelMetadata& meta,
                           const BoardState& state) const
{
  ParsedState p;
  if (!ParsePair (state, p))
    return nullptr;

  return std::make_unique<AdditionState> (*this, channelId, meta, p);
}

ChannelProtoVersion
AdditionRules::GetProtoVersion (const proto::ChannelMetadata& meta) const
{
  return ChannelProtoVersion::ORIGINAL;
}

Json::Value
AdditionChannel::ResolutionMove (const uint256& channelId,
                                 const proto::StateProof& proof) const
{
  Json::Value res(Json::objectValue);
  res["type"] = "resolution";
  res["id"] = channelId.ToHex ();
  res["proof"] = ProtoToBase64 (proof);

  return res;
}

Json::Value
AdditionChannel::DisputeMove (const uint256& channelId,
                              const proto::StateProof& proof) const
{
  Json::Value res(Json::objectValue);
  res["type"] = "dispute";
  res["id"] = channelId.ToHex ();
  res["proof"] = ProtoToBase64 (proof);

  return res;
}

bool
AdditionChannel::MaybeAutoMove (const ParsedBoardState& state, BoardMove& mv)
{
  if (!automovesEnabled)
    return false;

  const auto& addState = dynamic_cast<const AdditionState&> (state);
  return addState.MaybeAutoMove (mv);
}

void
AdditionChannel::MaybeOnChainMove (const ParsedBoardState& state,
                                   MoveSender& sender)
{
  const auto& addState = dynamic_cast<const AdditionState&> (state);
  addState.MaybeOnChainMove (sender);
}

/* ************************************************************************** */

MockTransactionSender::MockTransactionSender ()
{
  /* By default, we expect no calls to be made.  */
  EXPECT_CALL (*this, SendRawMove (_, _)).Times (0);
}

void
MockTransactionSender::ExpectFailure (
    const std::string& name, const testing::Matcher<const std::string&>& m)
{
  EXPECT_CALL (*this, SendRawMove (name, m))
      .WillOnce (Throw (std::runtime_error ("faked error")));
}

std::vector<uint256>
MockTransactionSender::ExpectSuccess (
    const unsigned n,
    const std::string& name, const testing::Matcher<const std::string&>& m)
{
  std::vector<uint256> txids;
  for (unsigned i = 0; i < n; ++i)
    {
      ++cnt;
      std::ostringstream str;
      str << "txid " << cnt;
      const uint256 txid = SHA256::Hash (str.str ());

      txids.push_back (txid);
      txidQueue.push (txid);
    }

  EXPECT_CALL (*this, SendRawMove (name, m))
      .Times (n)
      .WillRepeatedly (InvokeWithoutArgs ([&] ()
        {
          const uint256 txid = txidQueue.front ();
          txidQueue.pop ();

          mempool.insert (txid);
          return txid;
        }));

  return txids;
}

uint256
MockTransactionSender::ExpectSuccess (
    const std::string& name, const testing::Matcher<const std::string&>& m)
{
  const auto txids = ExpectSuccess (1, name, m);
  CHECK_EQ (txids.size (), 1);
  return txids[0];
}

void
MockTransactionSender::ClearMempool ()
{
  LOG (INFO) << "Clearing simulated mempool of MockTransactionSender";
  mempool.clear ();
}

bool
MockTransactionSender::IsPending (const uint256& txid) const
{
  return mempool.count (txid) > 0;
}

void
MockSignatureVerifier::SetValid (const std::string& sgn,
                                 const std::string& addr)
{
  EXPECT_CALL (*this, RecoverSigner (_, sgn)).WillRepeatedly (Return (addr));
}

void
MockSignatureVerifier::ExpectOne (const uint256& channelId,
                                  const proto::ChannelMetadata& meta,
                                  const std::string& topic,
                                  const std::string& msg,
                                  const std::string& sgn,
                                  const std::string& addr)
{
  const std::string hashed
      = GetChannelSignatureMessage (channelId, meta, topic, msg);
  EXPECT_CALL (*this, RecoverSigner (hashed, sgn)).WillOnce (Return (addr));
}

/* ************************************************************************** */

void
TestGame::SetupSchema (SQLiteDatabase& db)
{
  SetupGameChannelsSchema (db);
}

void
TestGame::GetInitialStateBlock (unsigned& height, std::string& hashHex) const
{
  LOG (FATAL) << "TestGame::GetInitialStateBlock is not implemented";
}

void
TestGame::InitialiseState (SQLiteDatabase& db)
{
  LOG (FATAL) << "TestGame::InitialiseState is not implemented";
}

void
TestGame::UpdateState (SQLiteDatabase& db, const Json::Value& blockData)
{
  LOG (FATAL) << "TestGame::UpdateState is not implemented";
}

Json::Value
TestGame::GetStateAsJson (const SQLiteDatabase& db)
{
  LOG (FATAL) << "TestGame::GetStateAsJson is not implemented";
}

const SignatureVerifier&
TestGame::GetSignatureVerifier ()
{
  return mockVerifier;
}

const BoardRules&
TestGame::GetBoardRules () const
{
  return rules;
}

TestGameFixture::TestGameFixture ()
  : game(verifier)
{
  game.Initialise (":memory:");
  game.InitialiseGameContext (Chain::MAIN, "add", nullptr);
  game.GetStorage ().Initialise ();
  /* The initialisation above already sets up the database schema.  */
}

SQLiteDatabase&
TestGameFixture::GetDb ()
{
  return game.GetDatabaseForTesting ();
}

/* ************************************************************************** */

} // namespace xaya
