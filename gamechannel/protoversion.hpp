// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_PROTOVERSION_HPP
#define GAMECHANNEL_PROTOVERSION_HPP

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <google/protobuf/message.h>

namespace xaya
{

class BoardRules;

/**
 * Protocol buffers have the property that they remain compatible in binary
 * format when new fields are added, which makes them useful for upgrading
 * protocols over time.  In a blockchain consensus environment, however,
 * we do not want "silent upgrades".  Instead, we need to control exactly
 * what rules are in effect at what time.
 *
 * Since SignedData and StateProof protos are used for game channels at the
 * consensus layer (at least potentially), we need to make sure that games
 * have full control over what "version" of those protos to accept at what
 * time in case we update or extend the format for those protos in the
 * game-channel framework.
 *
 * Hence, we define concrete and "fixed" versions, which are enumerated by
 * the enum values below.  Then, when games parse a protocol buffer from
 * a move (or otherwise obtain it), they can choose to explicitly enforce that
 * it matches a given version they want using the CheckProtoVersion function
 * before passing it to a game-channel function.
 */
enum class ChannelProtoVersion
{

  /**
   * The original version of SignedData and StateProofs, as first released
   * in the game-channel framework and the Xayaships tech demo.
   */
  ORIGINAL,

};

/**
 * Checks that the passed-in protocol buffer is valid for the fixed protocol
 * version given.  This e.g. checks that no newer fields are present.
 * The template is implemented for SignedData and StateProof.
 *
 * This function is used mainly internally.  External callers should likely
 * use CheckVersionedProto instead, which also enforces that there must not
 * be any unknown fields.
 */
template <typename Proto>
  bool CheckProtoVersion (ChannelProtoVersion version, const Proto& msg);

/**
 * Checks whether this message or any contained submessages have any unknown
 * fields set.
 */
bool HasAnyUnknownFields (const google::protobuf::Message& msg);

/**
 * Checks if a given proto (StateProof or SignedData) is valid with respect
 * to the version expected.  It also must not have any unknown fields.
 */
template <typename Proto>
  bool CheckVersionedProto (const BoardRules& rules,
                            const proto::ChannelMetadata& meta,
                            const Proto& msg);

} // namespace xaya

#endif // GAMECHANNEL_PROTOVERSION_HPP
