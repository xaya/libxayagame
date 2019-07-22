// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protoversion.hpp"

#include "proto/signatures.pb.h"
#include "proto/stateproof.pb.h"

#include <glog/logging.h>

#include <vector>

namespace xaya
{

using google::protobuf::FieldDescriptor;
using google::protobuf::Message;

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

bool
HasAnyUnknownFields (const Message& msg)
{
  const auto& reflection = *msg.GetReflection ();
  if (!reflection.GetUnknownFields (msg).empty ())
    return true;

  std::vector<const FieldDescriptor*> fields;
  reflection.ListFields (msg, &fields);

  for (const auto* f : fields)
    switch (f->type ())
      {
      case FieldDescriptor::TYPE_GROUP:
        LOG (FATAL) << "group is not allowed in game-channel protocol buffers";

      case FieldDescriptor::TYPE_MESSAGE:
        if (f->is_repeated ())
          for (int i = 0; i < reflection.FieldSize (msg, f); ++i)
            {
              const auto& nested = reflection.GetRepeatedMessage (msg, f, i);
              if (HasAnyUnknownFields (nested))
                return true;
            }
        else
          {
            const auto& nested = reflection.GetMessage (msg, f);
            if (HasAnyUnknownFields (nested))
              return true;
          }

      default:
        /* Just ignore any other types, as non-message fields cannot contain
           any nested unknown fields.  */
        break;
      }

  return false;
}

} // namespace xaya
