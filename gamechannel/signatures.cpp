// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include <xayautil/base64.hpp>
#include <xayautil/hash.hpp>

#include <glog/logging.h>

namespace xaya
{

std::string
GetChannelSignatureMessage (const std::string& gameId,
                            const uint256& channelId,
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
