#include "gamestatejson.hpp"

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

Json::Value
ChannelToGameStateJson (const ChannelData& ch, const BoardRules& r)
{
  const auto& id = ch.GetId ();
  const auto& meta = ch.GetMetadata ();

  Json::Value res(Json::objectValue);
  res["id"] = id.ToHex ();
  if (ch.HasDispute ())
    res["disputeheight"] = static_cast<int> (ch.GetDisputeHeight ());
  res["meta"] = ChannelMetadataToJson (meta);

  res["state"] = BoardStateToJson (r, id, meta, ch.GetLatestState ());
  res["state"]["proof"] = ProtoToBase64 (ch.GetStateProof ());
  res["reinit"] = BoardStateToJson (r, id, meta, ch.GetReinitState ());

  return res;
}

Json::Value
AllChannelsGameStateJson (ChannelsTable& tbl, const BoardRules& r)
{
  Json::Value res(Json::objectValue);
  auto stmt = tbl.QueryAll ();
  while (stmt.Step ())
    {
      auto h = tbl.GetFromResult (stmt);
      res[h->GetId ().ToHex ()] = ChannelToGameStateJson (*h, r);
    }

  return res;
}

} // namespace xaya
