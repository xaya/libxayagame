// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protoutils.hpp"

#include "proto/metadata.pb.h"
#include "proto/stateproof.pb.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <gtest/gtest.h>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;
using google::protobuf::util::MessageDifferencer;

class ProtoUtilsTests : public testing::Test
{

protected:

  /**
   * Performs a roundtrip of encoding and decoding the given protocol buffer
   * message to/from base64.  Checks that the resulting value is equal
   * to the original input.
   */
  template <typename Proto>
    void
    CheckRoundtrip (const Proto& input)
  {
    const std::string encoded = ProtoToBase64 (input);

    Proto output;
    ASSERT_TRUE (ProtoFromBase64 (encoded, output));

    ASSERT_TRUE (MessageDifferencer::Equals (input, output));
  }

};

TEST_F (ProtoUtilsTests, MetadataRoundtrip)
{
  proto::ChannelMetadata meta;
  CHECK (TextFormat::ParseFromString (R"(
    reinit: "foo"
    participants:
      {
        name: "player 1"
        address: "my address"
      }
    participants:
      {
        name: "player 2"
        address: "some other address"
      }
  )", &meta));
  CheckRoundtrip (meta);
}

TEST_F (ProtoUtilsTests, StateProofRoundtrip)
{
  proto::StateProof proof;
  CHECK (TextFormat::ParseFromString (R"(
    initial_state:
      {
        data: "1 2"
        signatures: "sgn"
      }
    transitions:
      {
        move: "42"
        new_state:
          {
            data: "100 5"
            signatures: "a"
            signatures: "b"
          }
      }
  )", &proof));
  CheckRoundtrip (proof);
}

} // anonymous namespace
} // namespace xaya
