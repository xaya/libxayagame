// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include <xayagame/signatures.hpp>
#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <string>

namespace xaya
{

std::set<int>
VerifyParticipantSignatures (XayaRpcClient& rpc,
                             const proto::ChannelMetadata& meta,
                             const proto::SignedData& data)
{
  const std::string msg = SHA256::Hash (data.data ()).ToHex ();

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
