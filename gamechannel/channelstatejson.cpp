// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelstatejson.hpp"

#include "protoutils.hpp"

#include <xayautil/base64.hpp>

#include <glog/logging.h>

namespace xaya
{

Json::Value
ChannelMetadataToJson (const proto::ChannelMetadata& meta)
{
  Json::Value res(Json::objectValue);

  Json::Value participants(Json::arrayValue);
  for (const auto& p : meta.participants ())
    {
      Json::Value cur(Json::objectValue);
      cur["name"] = p.name ();
      cur["address"] = p.address ();
      participants.append (cur);
    }
  res["participants"] = participants;

  res["reinit"] = EncodeBase64 (meta.reinit ());
  res["proto"] = ProtoToBase64 (meta);

  return res;
}

Json::Value
BoardStateToJson (const BoardRules& r,
                  const uint256& channelId, const proto::ChannelMetadata& meta,
                  const BoardState& state)
{
  Json::Value res(Json::objectValue);
  res["base64"] = EncodeBase64 (state);

  auto parsed = r.ParseState (channelId, meta, state);
  CHECK (parsed != nullptr)
      << "Channel " << channelId.ToHex () << " has invalid state: "
      << state;

  const Json::Value parsedJson = parsed->ToJson ();
  if (!parsedJson.isNull ())
    res["parsed"] = parsedJson;

  res["whoseturn"] = Json::Value ();
  const int turn = parsed->WhoseTurn ();
  if (turn != ParsedBoardState::NO_TURN)
    res["whoseturn"] = turn;
  res["turncount"] = static_cast<int> (parsed->TurnCount ());

  return res;
}

} // namespace xaya
