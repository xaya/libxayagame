/*
 * WASM shim for gamechannel/protoversion.cpp.
 * Provides simplified implementations of the proto version checking
 * functions for the WASM environment.
 *
 * In the browser, proto version validation is not needed because:
 * - Data comes from known channel participants (peers).
 * - The server-side GSP performs full validation.
 * - The WASM module only needs to apply moves and verify signatures.
 *
 * All checks unconditionally pass. This avoids pulling in the full
 * protobuf reflection API (GetReflection, UnknownFields) which would
 * increase the WASM binary size.
 */

#include <gamechannel/protoversion.hpp>
#include <gamechannel/boardrules.hpp>

namespace xaya
{

/* Always returns false: trust the data in WASM. */
bool HasAnyUnknownFields (const google::protobuf::Message&)
{
  return false;
}

template <>
bool CheckProtoVersion<proto::SignedData> (ChannelProtoVersion, const proto::SignedData&)
{
  return true;
}

template <>
bool CheckProtoVersion<proto::StateProof> (ChannelProtoVersion, const proto::StateProof&)
{
  return true;
}

template <>
bool CheckVersionedProto<proto::SignedData> (const BoardRules&,
                                              const proto::ChannelMetadata&,
                                              const proto::SignedData&)
{
  return true;
}

template <>
bool CheckVersionedProto<proto::StateProof> (const BoardRules&,
                                              const proto::ChannelMetadata&,
                                              const proto::StateProof&)
{
  return true;
}

} // namespace xaya
