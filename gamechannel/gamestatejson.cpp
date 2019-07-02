#include "gamestatejson.hpp"

#include <xayautil/base64.hpp>

#include <glog/logging.h>

#include <sqlite3.h>

namespace xaya
{

namespace
{

/**
 * Encodes a protocol buffer as base64 string.
 */
template <typename Proto>
  std::string
  EncodeProto (const Proto& msg)
{
  std::string serialised;
  CHECK (msg.SerializeToString (&serialised));
  return EncodeBase64 (serialised);
}

/**
 * Encodes a board state as JSON object.
 */
Json::Value
EncodeBoardState (const ChannelData& ch, const BoardRules& r,
                  const BoardState& state)
{
  const auto& id = ch.GetId ();

  Json::Value res(Json::objectValue);
  auto parsed = r.ParseState (id, ch.GetMetadata (), state);
  CHECK (parsed != nullptr)
      << "Channel " << id.ToHex () << " has invalid state on chain: "
      << state;
  res["data"] = parsed->ToJson ();

  res["whoseturn"] = Json::Value ();
  const int turn = parsed->WhoseTurn ();
  if (turn != ParsedBoardState::NO_TURN)
    res["whoseturn"] = turn;
  res["turncount"] = static_cast<int> (parsed->TurnCount ());

  return res;
}

} // anonymous namespace

Json::Value
ChannelToGameStateJson (const ChannelData& ch, const BoardRules& r)
{
  const auto& id = ch.GetId ();
  const auto& metaPb = ch.GetMetadata ();

  Json::Value res(Json::objectValue);
  res["id"] = id.ToHex ();
  if (ch.HasDispute ())
    res["disputeheight"] = static_cast<int> (ch.GetDisputeHeight ());

  Json::Value meta(Json::objectValue);
  Json::Value participants(Json::arrayValue);
  for (const auto& p : metaPb.participants ())
    {
      Json::Value cur(Json::objectValue);
      cur["name"] = p.name ();
      cur["address"] = p.address ();
      participants.append (cur);
    }
  meta["participants"] = participants;
  meta["reinit"] = EncodeBase64 (metaPb.reinit ());
  meta["proto"] = EncodeProto (metaPb);
  res["meta"] = meta;

  res["state"] = EncodeBoardState (ch, r, ch.GetLatestState ());
  res["state"]["proof"] = EncodeProto (ch.GetStateProof ());
  res["reinit"] = EncodeBoardState (ch, r, ch.GetReinitState ());

  return res;
}

Json::Value
AllChannelsGameStateJson (ChannelsTable& tbl, const BoardRules& r)
{
  Json::Value res(Json::objectValue);
  auto* stmt = tbl.QueryAll ();
  while (true)
    {
      const int rc = sqlite3_step (stmt);
      if (rc == SQLITE_DONE)
        break;
      CHECK_EQ (rc, SQLITE_ROW);

      auto h = tbl.GetFromResult (stmt);
      res[h->GetId ().ToHex ()] = ChannelToGameStateJson (*h, r);
    }

  return res;
}

} // namespace xaya
