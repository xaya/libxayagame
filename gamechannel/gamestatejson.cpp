#include "gamestatejson.hpp"

#include <glog/logging.h>

#include <sqlite3.h>

namespace xaya
{

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
  res["meta"] = meta;

  Json::Value state(Json::objectValue);
  auto parsed = r.ParseState (id, metaPb, ch.GetState ());
  CHECK (parsed != nullptr)
      << "Channel " << id.ToHex () << " has invalid state on chain: "
      << ch.GetState ();
  state["data"] = parsed->ToJson ();

  state["whoseturn"] = Json::Value ();
  const int turn = parsed->WhoseTurn ();
  if (turn != ParsedBoardState::NO_TURN)
    state["whoseturn"] = turn;
  state["turncount"] = static_cast<int> (parsed->TurnCount ());
  res["state"] = state;

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
