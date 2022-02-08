// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <glog/logging.h>

#include <sstream>

namespace xaya
{

std::string
GetChannelSignatureMessage (const std::string& gameId,
                            const uint256& channelId,
                            const proto::ChannelMetadata& meta,
                            const std::string& topic,
                            const std::string& data)
{
  for (const char t : topic)
    CHECK ((t >= '0' && t <= '9')
              || (t >= 'A' && t <= 'Z')
              || (t >= 'a' && t <= 'z'))
      << "Topic string contains invalid character: " << topic;

  std::ostringstream res;
  res << "Game-Channel Signature\n"
      << "Game ID: " << gameId << "\n"
      << "Channel: " << channelId.ToHex () << "\n"
      << "Reinit: " << EncodeBase64 (meta.reinit ()) << "\n"
      << "Topic: " << topic << "\n"
      << "Data Hash: " << SHA256::Hash (data).ToHex ();

  return res.str ();
}

std::set<int>
VerifyParticipantSignatures (const SignatureVerifier& verifier,
                             const std::string& gameId,
                             const uint256& channelId,
                             const proto::ChannelMetadata& meta,
                             const std::string& topic,
                             const proto::SignedData& data)
{
  const auto msg
      = GetChannelSignatureMessage (gameId, channelId, meta,
                                    topic, data.data ());

  std::set<std::string> addresses;
  for (const auto& sgn : data.signatures ())
    addresses.emplace (verifier.RecoverSigner (msg, sgn));

  std::set<int> res;
  for (int i = 0; i < meta.participants_size (); ++i)
    if (addresses.count (meta.participants (i).address ()) > 0)
      res.emplace (i);

  return res;
}

bool
SignDataForParticipant (SignatureSigner& signer,
                        const std::string& gameId,
                        const uint256& channelId,
                        const proto::ChannelMetadata& meta,
                        const std::string& topic,
                        const int index,
                        proto::SignedData& data)
{
  CHECK_GE (index, 0);
  CHECK_LT (index, meta.participants_size ());
  const std::string& addr = meta.participants (index).address ();
  LOG (INFO) << "Trying to sign data with address " << addr << "...";

  if (addr != signer.GetAddress ())
    {
      LOG (ERROR) << "The provided signer is for a different address";
      return false;
    }

  const auto msg
      = GetChannelSignatureMessage (gameId, channelId, meta,
                                    topic, data.data ());
  data.add_signatures (signer.SignMessage (msg));
  return true;
}

} // namespace xaya
