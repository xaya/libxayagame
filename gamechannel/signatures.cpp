// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include <xayagame/signatures.hpp>
#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <glog/logging.h>

#include <string>

namespace xaya
{

std::string
GetChannelSignatureMessage (const uint256& channelId,
                            const proto::ChannelMetadata& meta,
                            const std::string& topic,
                            const std::string& data)
{
  CHECK_EQ (topic.find ('\0'), std::string::npos)
      << "Topic string contains nul character";
  const std::string nulByte("\0", 1);

  SHA256 hasher;
  hasher << channelId;
  hasher << EncodeBase64 (meta.reinit ()) << nulByte;
  hasher << topic << nulByte;
  hasher << data;

  return hasher.Finalise ().ToHex ();
}

std::set<int>
VerifyParticipantSignatures (XayaRpcClient& rpc,
                             const uint256& channelId,
                             const proto::ChannelMetadata& meta,
                             const std::string& topic,
                             const proto::SignedData& data)
{
  const std::string msg = GetChannelSignatureMessage (channelId, meta, topic,
                                                      data.data ());

  std::set<std::string> addresses;
  for (const auto& sgn : data.signatures ())
    {
      const std::string& sgnStr = EncodeBase64 (sgn);
      addresses.emplace (VerifyMessage (rpc, msg, sgnStr));
    }

  std::set<int> res;
  for (int i = 0; i < meta.participants_size (); ++i)
    if (addresses.count (meta.participants (i).address ()) > 0)
      res.emplace (i);

  return res;
}

} // namespace xaya
