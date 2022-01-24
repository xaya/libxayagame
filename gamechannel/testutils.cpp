// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include <xayautil/hash.hpp>

#include <gmock/gmock.h>

#include <glog/logging.h>

#include <sstream>

namespace xaya
{

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::Throw;

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

} // namespace xaya
