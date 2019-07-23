// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protoversion.hpp"

#include "proto/signatures.pb.h"
#include "proto/stateproof.pb.h"
#include "proto/testprotos.pb.h"
#include "testgame.hpp"

#include <google/protobuf/text_format.h>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <string>

namespace xaya
{
namespace
{

using google::protobuf::TextFormat;

/* ************************************************************************** */

class CheckProtoVersionTests : public testing::Test
{

protected:

  /**
   * Parses a text proto and checks whether it matches the given version.
   */
  template <typename Proto>
    static bool
    CheckText (const ChannelProtoVersion version, const std::string& str)
  {
    Proto msg;
    CHECK (TextFormat::ParseFromString (str, &msg));

    return CheckProtoVersion (version, msg);
  }

};

TEST_F (CheckProtoVersionTests, SignedData)
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

TEST_F (CheckProtoVersionTests, StateProof)
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

/* ************************************************************************** */

class HasAnyUnknownFieldsTests : public testing::Test
{

protected:

  /**
   * Parses a text format ExtendedUnknownFieldTest proto and "converts" it
   * to UnknownFieldTest.  Thereby, the extra fields that have been set will
   * become unknown fields.  Then, returns the result of HasAnyUnknownFields
   * on the resulting message.
   */
  static bool
  HasUnknownFields (const std::string& str)
  {
    proto::ExtendedUnknownFieldTest extended;
    CHECK (TextFormat::ParseFromString (str, &extended));

    std::string serialised;
    CHECK (extended.SerializeToString (&serialised));

    proto::UnknownFieldTest basic;
    CHECK (basic.ParseFromString (serialised));

    return HasAnyUnknownFields (basic);
  }

};

TEST_F (HasAnyUnknownFieldsTests, NoUnknownFields)
{
  EXPECT_FALSE (HasUnknownFields (R"(
    single_int: 42
    single_str: "foo"
    single_msg:
      {
        repeated_int: 5
        repeated_int: 7
        repeated_str: "bar"
        repeated_str: "baz"
        repeated_msg: {}
        repeated_msg:
          {
            single_msg: {}
          }
      }
  )"));
}

TEST_F (HasAnyUnknownFieldsTests, TopLevelFields)
{
  EXPECT_TRUE (HasUnknownFields (R"(
    unknown_int: 5
  )"));
  EXPECT_TRUE (HasUnknownFields (R"(
    unknown_msg: {}
  )"));

  EXPECT_TRUE (HasUnknownFields (R"(
    unknown_repeated_int: 0
  )"));
  EXPECT_TRUE (HasUnknownFields (R"(
    unknown_repeated_msg: {}
  )"));
}

TEST_F (HasAnyUnknownFieldsTests, InNestedMessage)
{
  EXPECT_TRUE (HasUnknownFields (R"(
    single_msg:
      {
        repeated_msg: {}
        repeated_msg:
          {
            unknown_int: 0
          }
      }
  )"));

  EXPECT_TRUE (HasUnknownFields (R"(
    repeated_msg: {}
    repeated_msg:
      {
        single_msg:
          {
            unknown_msg: {}
          }
      }
  )"));
}

/* ************************************************************************** */

class CheckVersionedProtoTests : public TestGameFixture
{

protected:

  proto::ChannelMetadata meta;
  proto::StateProof proof;

  CheckVersionedProtoTests ()
  {
    CHECK (TextFormat::ParseFromString (R"(
      initial_state:
        {
          data: "1 2"
          signatures: "signature"
        }
    )", &proof));
  }

};

TEST_F (CheckVersionedProtoTests, Valid)
{
  EXPECT_TRUE (CheckVersionedProto (game.rules, meta, proof));
  EXPECT_TRUE (CheckVersionedProto (game.rules, meta, proof.initial_state ()));
}

TEST_F (CheckVersionedProtoTests, InvalidVersion)
{
  proof.mutable_initial_state ()->set_for_testing_version ("foo");
  EXPECT_FALSE (CheckVersionedProto (game.rules, meta, proof));
  EXPECT_FALSE (CheckVersionedProto (game.rules, meta, proof.initial_state ()));
}

TEST_F (CheckVersionedProtoTests, UnknownField)
{
  auto state = proof.initial_state ();
  state.GetReflection ()->MutableUnknownFields (&state)->AddVarint (123456, 42);
  EXPECT_FALSE (CheckVersionedProto (game.rules, meta, state));

  proof.GetReflection ()->MutableUnknownFields (&proof)->AddVarint (123456, 42);
  EXPECT_FALSE (CheckVersionedProto (game.rules, meta, proof));
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
