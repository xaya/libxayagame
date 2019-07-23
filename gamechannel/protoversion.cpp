// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protoversion.hpp"

#include "boardrules.hpp"
#include "proto/signatures.pb.h"

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

namespace
{

template <typename Proto>
  bool
  InternalCheckVersionedProto (const BoardRules& rules,
                               const proto::ChannelMetadata& meta,
                               const Proto& msg)
{
  if (HasAnyUnknownFields (msg))
    {
      LOG (WARNING)
          << "Provided proto has unknown fields:\n" << msg.DebugString ();
      return false;
    }

  const auto expectedVersion = rules.GetProtoVersion (meta);
  if (!CheckProtoVersion (expectedVersion, msg))
    {
      LOG (WARNING)
          << "Message does not match expected version "
          << static_cast<int> (expectedVersion) << ":\n" << msg.DebugString ();
      return false;
    }

  return true;
}

} // anonymous namespace

/* We have to use full explicit specialisations, since defining a general
   template in the header file would not work due to the cyclic dependency
   with boardrules.hpp.  But since we only need the function for two concrete
   protos anyway, that is fine and does not involve any code duplication.  */

template <>
  bool
  CheckVersionedProto (const BoardRules& rules,
                       const proto::ChannelMetadata& meta,
                       const proto::StateProof& msg)
{
  return InternalCheckVersionedProto (rules, meta, msg);
}

template <>
  bool
  CheckVersionedProto (const BoardRules& rules,
                       const proto::ChannelMetadata& meta,
                       const proto::SignedData& msg)
{
  return InternalCheckVersionedProto (rules, meta, msg);
}

} // namespace xaya
