// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protoversion.hpp"

#include "proto/signatures.pb.h"
#include "proto/stateproof.pb.h"

#include <google/protobuf/text_format.h>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <string>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;

class ProtoVersionTests : public testing::Test
{

protected:

  /**
   * Parses a text proto and checks whether it matches the given version.
   */
  template <typename Proto>
    bool
    CheckText (const ChannelProtoVersion version, const std::string& str) const
  {
    Proto msg;
    CHECK (TextFormat::ParseFromString (str, &msg));

    return CheckProtoVersion (version, msg);
  }

};

TEST_F (ProtoVersionTests, SignedData)
{
  EXPECT_TRUE (CheckText<proto::SignedData> (ChannelProtoVersion::ORIGINAL,
      R"(
        data: "foo"
        signatures: "sgn"
      )"));
  EXPECT_FALSE (CheckText<proto::SignedData> (ChannelProtoVersion::ORIGINAL,
      R"(
        data: "foo"
        signatures: "sgn"
        for_testing_version: "xyz"
      )"));
}

TEST_F (ProtoVersionTests, StateProof)
{
  EXPECT_TRUE (CheckText<proto::StateProof> (ChannelProtoVersion::ORIGINAL,
      R"(
        initial_state:
          {
            data: "foo"
            signatures: "sgn"
          }
        transitions:
          {
            move: "mv"
            new_state:
              {
                data: "bar"
                signatures: "signed as well"
              }
          }
      )"));

  EXPECT_FALSE (CheckText<proto::StateProof> (ChannelProtoVersion::ORIGINAL,
      R"(
        initial_state:
          {
            data: "foo"
            signatures: "sgn"
            for_testing_version: "xyz"
          }
      )"));

  EXPECT_FALSE (CheckText<proto::StateProof> (ChannelProtoVersion::ORIGINAL,
      R"(
        transitions:
          {
            move: "mv"
            new_state:
              {
                data: "bar"
                signatures: "signed as well"
                for_testing_version: "xyz"
              }
          }
      )"));
}

} // anonymous namespace
} // namespace xaya
