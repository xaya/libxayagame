// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ethsignatures.hpp"

#include <eth-utils/hexutils.hpp>

#include <gtest/gtest.h>

namespace xaya
{
namespace
{

class EthSignaturesTests : public testing::Test
{

protected:

  /** A particular Ethereum private key.  */
  static const std::string KEY;
  /** The corresponding address.  */
  static const std::string ADDR;

  const ethutils::ECDSA ctx;
  const EthSignatureVerifier verifier;

  /** A signer based on our example private key.  */
  EthSignatureSigner signer;

  EthSignaturesTests ()
    : verifier(ctx), signer(ctx, KEY)
  {}

};

const std::string EthSignaturesTests::KEY
    = "0x6abe12d46d022b3fcb0209db3ee70791c7957578024158b487f68c50d793dac1";
const std::string EthSignaturesTests::ADDR
    = "0x16cf19A9EF7E413435DEc2a38FA9664Cb2BfAb15";

TEST_F (EthSignaturesTests, SignerAddress)
{
  EXPECT_EQ (signer.GetAddress (), ADDR);
}

TEST_F (EthSignaturesTests, SignatureSize)
{
  const std::string sgn = signer.SignMessage ("foo");
  EXPECT_EQ (sgn.size (), 65);
}

TEST_F (EthSignaturesTests, InvalidSignature)
{
  const std::string sgn = signer.SignMessage ("foo");

  for (const auto& t : {std::string (), std::string ("foo"),
                        std::string (65, '\0'), std::string (65, '\xFF'),
                        std::string (64, '\0') + std::string (1, 27),
                        ethutils::Hexlify (sgn),
                        "0x" + ethutils::Hexlify (sgn)})
    EXPECT_EQ (verifier.RecoverSigner ("foo", t), "invalid");
}

TEST_F (EthSignaturesTests, Roundtrip)
{
  const std::string sgn = signer.SignMessage ("foo");
  EXPECT_EQ (verifier.RecoverSigner ("foo", sgn), ADDR);
  EXPECT_NE (verifier.RecoverSigner ("bar", sgn), ADDR);
}

} // anonymous namespace
} // namespace xaya
