// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protoversion.hpp"

#include "proto/signatures.pb.h"
#include "proto/stateproof.pb.h"

#include <glog/logging.h>

namespace xaya
{

template <>
  bool
  CheckProtoVersion (const ChannelProtoVersion version,
                     const proto::SignedData& msg)
{
  switch (version)
    {
    case ChannelProtoVersion::ORIGINAL:
      return !msg.has_for_testing_version ();

    default:
      LOG (FATAL) << "Invalid target version: " << static_cast<int> (version);
    }
}

template <>
  bool
  CheckProtoVersion (const ChannelProtoVersion version,
                     const proto::StateProof& msg)
{
  if (!CheckProtoVersion (version, msg.initial_state ()))
    return false;

  for (const auto& t : msg.transitions ())
    if (!CheckProtoVersion (version, t.new_state ()))
      return false;

  return true;
}

} // namespace xaya
