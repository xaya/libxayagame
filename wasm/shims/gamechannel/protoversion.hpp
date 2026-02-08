/*
 * WASM replacement for gamechannel/protoversion.hpp.
 * Same interface as the original, but does not include
 * proto/signatures.pb.h (which would pull in eth-utils
 * dependencies not available in WASM).
 *
 * The corresponding implementation (protoversion_shim.cpp)
 * provides simplified always-pass checks.
 */

#ifndef GAMECHANNEL_PROTOVERSION_HPP
#define GAMECHANNEL_PROTOVERSION_HPP

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <google/protobuf/message.h>

namespace xaya
{

class BoardRules;

enum class ChannelProtoVersion
{
  ORIGINAL,
};

template <typename Proto>
  bool CheckProtoVersion (ChannelProtoVersion version, const Proto& msg);

bool HasAnyUnknownFields (const google::protobuf::Message& msg);

template <typename Proto>
  bool CheckVersionedProto (const BoardRules& rules,
                            const proto::ChannelMetadata& meta,
                            const Proto& msg);

} // namespace xaya

#endif // GAMECHANNEL_PROTOVERSION_HPP
