// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamestatejson.hpp"

#include "channelstatejson_tests.hpp"

namespace xaya
{
namespace
{

class GameStateJsonTests : public ChannelStateJsonTests
{

protected:

  ChannelsTable tbl;

  GameStateJsonTests ()
    : tbl(GetDb ())
  {
    auto h = tbl.CreateNew (id1);
    h->Reinitialise (meta1, "100 2");
    h.reset ();

    h = tbl.CreateNew (id2);
    h->SetDisputeHeight (55);
    h->Reinitialise (meta2, "40 10");
    proto::StateProof proof;
    proof.mutable_initial_state ()->set_data ("50 20");
    h->SetStateProof (proof);
    h.reset ();
  }

};

TEST_F (GameStateJsonTests, WithoutDispute)
{
  auto h = tbl.GetById (id1);
  CheckChannelJson (ChannelToGameStateJson (*h, game.rules), R"(
    {
      "state":
        {
          "parsed": {"count": 2, "number": 100},
          "turncount": 2,
          "whoseturn": null
        },
      "reinit":
        {
          "parsed": {"count": 2, "number": 100},
          "turncount": 2,
          "whoseturn": null
        }
    }
  )", id1, meta1, "100 2", "100 2");
}

TEST_F (GameStateJsonTests, WithDispute)
{
  auto h = tbl.GetById (id2);
  CheckChannelJson (ChannelToGameStateJson (*h, game.rules), R"(
    {
      "disputeheight": 55,
      "state":
        {
          "parsed": {"count": 20, "number": 50},
          "turncount": 20,
          "whoseturn": 0
        },
      "reinit":
        {
          "parsed": {"count": 10, "number": 40},
          "turncount": 10,
          "whoseturn": 0
        }
    }
  )", id2, meta2, "40 10", "50 20");
}

TEST_F (GameStateJsonTests, AllChannels)
{
  Json::Value expected(Json::objectValue);

  auto h = tbl.GetById (id1);
  expected[h->GetId ().ToHex ()] = ChannelToGameStateJson (*h, game.rules);
  h = tbl.GetById (id2);
  expected[h->GetId ().ToHex ()] = ChannelToGameStateJson (*h, game.rules);

  EXPECT_EQ (AllChannelsGameStateJson (tbl, game.rules), expected);
}

} // anonymous namespace
} // namespace xaya
