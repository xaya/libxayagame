// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include "gamestatejson.hpp"
#include "schema.hpp"

#include <gamechannel/database.hpp>
#include <gamechannel/proto/stateproof.pb.h>
#include <gamechannel/protoutils.hpp>

#include <glog/logging.h>

namespace ships
{

const xaya::BoardRules&
ShipsLogic::GetBoardRules () const
{
  return boardRules;
}

void
ShipsLogic::SetupSchema (xaya::SQLiteDatabase& db)
{
  SetupGameChannelsSchema (db);
  SetupShipsSchema (db);
}

void
ShipsLogic::GetInitialStateBlock (unsigned& height, std::string& hashHex) const
{
  const xaya::Chain chain = GetChain ();
  switch (chain)
    {
    case xaya::Chain::MAIN:
      height = 930000;
      hashHex
          = "0b615b33ad3b4da22af2fe53553fbcc49ae00e27ae04c1d7e275c2a76d8f87d9";
      break;

    case xaya::Chain::TEST:
      height = 40000;
      hashHex
          = "74240aba644be39551e74c52eb4ffe6b63d1453c7d4cd1f6cfdee30d55354394";
      break;

    case xaya::Chain::REGTEST:
      height = 0;
      hashHex
          = "6f750b36d22f1dc3d0a6e483af45301022646dfc3b3ba2187865f5a7d6d83ab1";
      break;

    default:
      LOG (FATAL) << "Invalid chain value: " << static_cast<int> (chain);
    }
}

void
ShipsLogic::InitialiseState (xaya::SQLiteDatabase& db)
{
  /* The game simply starts with an empty database.  No stats for any names
     yet, and also no channels defined.  */
}

namespace
{

/**
 * Tries to process a "create channel" move, if the JSON object describes
 * a valid one.
 */
void
HandleCreateChannel (xaya::SQLiteDatabase& db,
                     const Json::Value& obj,
                     const std::string& name,
                     const xaya::uint256& txid)
{
  if (!obj.isObject ())
    return;

  const auto& addrVal = obj["addr"];
  if (obj.size () != 1 || !addrVal.isString ())
    {
      LOG (WARNING) << "Invalid create channel move: " << obj;
      return;
    }
  const std::string addr = addrVal.asString ();

  LOG (INFO)
      << "Creating channel with ID " << txid.ToHex ()
      << " for user " << name << " with address " << addr;

  xaya::ChannelsTable tbl(db);

  /* Verify that this is indeed a new instance and not an existing one.  That
     should never happen, assuming that txid's do not collide.  */
  auto h = tbl.GetById (txid);
  CHECK (h == nullptr) << "Already have channel with ID " << txid.ToHex ();

  h = tbl.CreateNew (txid);
  xaya::proto::ChannelMetadata meta;
  auto* p = meta.add_participants ();
  p->set_name (name);
  p->set_address (addr);
  h->Reinitialise (meta, "");
}

/**
 * Helper method that tries to extract a channel ID in a move JSON object
 * and retrieve that channel.
 */
xaya::ChannelsTable::Handle
RetrieveChannelFromMove (const Json::Value& obj, xaya::ChannelsTable& tbl)
{
  CHECK (obj.isObject ());
  const auto& idVal = obj["id"];
  if (!idVal.isString ())
    {
      LOG (WARNING) << "No channel ID given: " << obj;
      return nullptr;
    }
  const std::string id = idVal.asString ();

  xaya::uint256 channelId;
  if (!channelId.FromHex (id))
    {
      LOG (WARNING) << "Invalid uint256 channel ID: " << id;
      return nullptr;
    }

  auto h = tbl.GetById (channelId);
  if (h == nullptr)
    {
      LOG (WARNING) << "Action for non-existant channel: " << id;
      return  nullptr;
    }

  return h;
}

/**
 * Tries to process a "join channel" move.
 */
void
HandleJoinChannel (xaya::SQLiteDatabase& db,
                   const Json::Value& obj, const std::string& name,
                   const xaya::uint256& txid)
{
  if (!obj.isObject ())
    return;

  const auto& addrVal = obj["addr"];
  if (obj.size () != 2 || !addrVal.isString ())
    {
      LOG (WARNING) << "Invalid join channel move: " << obj;
      return;
    }
  const std::string addr = addrVal.asString ();

  xaya::ChannelsTable tbl(db);
  auto h = RetrieveChannelFromMove (obj, tbl);
  if (h == nullptr)
    return;

  const auto& meta = h->GetMetadata ();
  if (meta.participants_size () != 1)
    {
      LOG (WARNING)
          << "Cannot join channel " << h->GetId ().ToHex ()
          << " with " << meta.participants_size () << " participants";
      return;
    }

  if (meta.participants (0).name () == name)
    {
      LOG (WARNING)
          << name << " cannot join channel " << h->GetId ().ToHex ()
          << " a second time";
      return;
    }

  LOG (INFO)
      << "Adding " << name << " to channel " << h->GetId ().ToHex ()
      << " with address " << addr;

  xaya::proto::ChannelMetadata newMeta = meta;
  xaya::UpdateMetadataReinit (txid, newMeta);
  auto* p = newMeta.add_participants ();
  p->set_name (name);
  p->set_address (addr);
  CHECK_EQ (newMeta.participants_size (), 2);

  xaya::BoardState state;
  CHECK (InitialBoardState ().SerializeToString (&state));
  h->Reinitialise (newMeta, state);
}

/**
 * Tries to process an "abort channel" move.
 */
void
HandleAbortChannel (xaya::SQLiteDatabase& db,
                    const Json::Value& obj, const std::string& name)
{
  if (!obj.isObject ())
    return;

  if (obj.size () != 1)
    {
      LOG (WARNING) << "Invalid abort channel move: " << obj;
      return;
    }

  xaya::ChannelsTable tbl(db);
  auto h = RetrieveChannelFromMove (obj, tbl);
  if (h == nullptr)
    return;

  const xaya::uint256 id = h->GetId ();
  const auto& meta = h->GetMetadata ();
  if (meta.participants_size () != 1)
    {
      LOG (WARNING)
          << "Cannot abort channel " << id.ToHex ()
          << " with " << meta.participants_size () << " participants";
      return;
    }

  if (meta.participants (0).name () != name)
    {
      LOG (WARNING)
          << name << " cannot abort channel " << id.ToHex ()
          << ", only " << meta.participants (0).name () << " can";
      return;
    }

  LOG (INFO) << "Aborting channel " << id.ToHex ();
  h.reset ();
  tbl.DeleteById (id);
}

/**
 * Extracts a base64-encoded, serialised proto from a JSON string, if possible.
 */
template <typename T>
  bool
  ExtractProto (const Json::Value& val, T& res)
{
  if (!val.isString ())
    return false;
  const std::string str = val.asString ();

  if (!xaya::ProtoFromBase64 (val.asString (), res))
    {
      LOG (WARNING) << "Could not get proto from base64 string";
      return false;
    }

  return true;
}

} // anonymous namespace

void
ShipsLogic::HandleCloseChannel (xaya::SQLiteDatabase& db,
                                const Json::Value& obj)
{
  if (!obj.isObject ())
    return;

  if (obj.size () != 2)
    {
      LOG (WARNING) << "Invalid close channel move: " << obj;
      return;
    }

  xaya::proto::SignedData data;
  if (!ExtractProto (obj["stmt"], data))
    {
      LOG (WARNING) << "Failed to extract SignedData from move: " << obj;
      return;
    }

  xaya::ChannelsTable tbl(db);
  auto h = RetrieveChannelFromMove (obj, tbl);
  if (h == nullptr)
    return;

  const xaya::uint256 id = h->GetId ();
  const auto& meta = h->GetMetadata ();
  if (meta.participants_size () != 2)
    {
      LOG (WARNING)
          << "Cannot close channel " << id.ToHex ()
          << " with " << meta.participants_size () << " participants";
      return;
    }

  proto::WinnerStatement stmt;
  if (!VerifySignedWinnerStatement (boardRules, GetXayaRpc (),
                                    id, meta, data, stmt))
    {
      LOG (WARNING)
          << "Winner statement for closing channel " << id.ToHex ()
          << " is invalid: " << obj;
      return;
    }

  LOG (INFO)
      << "Closing channel " << id.ToHex ()
      << " with winner " << stmt.winner ()
      << " (" << meta.participants (stmt.winner ()).name () << ")";

  UpdateStats (db, meta, stmt.winner ());
  h.reset ();
  tbl.DeleteById (id);
}

namespace
{

/**
 * Tries to parse a dispute or resolution move.  If successful (the move
 * is valid and the channel it refers to exists), this returns the channel's
 * handle and sets the StateProof proto that was retrieved from the move.
 * Otherwise, a null handle is returned.
 */
xaya::ChannelsTable::Handle
ParseDisputeResolutionMove (const Json::Value& obj, xaya::ChannelsTable& tbl,
                            xaya::proto::StateProof& proof)
{
  if (!obj.isObject ())
    return nullptr;

  if (obj.size () != 2)
    {
      LOG (WARNING) << "Invalid dispute/resolution move: " << obj;
      return nullptr;
    }

  if (!ExtractProto (obj["state"], proof))
    {
      LOG (WARNING) << "Failed to extract StateProof from move: " << obj;
      return nullptr;
    }

  return RetrieveChannelFromMove (obj, tbl);
}

} // anonymous namespace

void
ShipsLogic::HandleDisputeResolution (xaya::SQLiteDatabase& db,
                                     const Json::Value& obj,
                                     const unsigned height,
                                     const bool isDispute)
{
  xaya::ChannelsTable tbl(db);
  xaya::proto::StateProof proof;
  auto h = ParseDisputeResolutionMove (obj, tbl, proof);
  if (h == nullptr)
    return;

  const xaya::uint256 id = h->GetId ();
  const auto& meta = h->GetMetadata ();
  if (meta.participants_size () != 2)
    {
      LOG (WARNING)
          << "Cannot file dispute/resolution for channel " << id.ToHex ()
          << " with " << meta.participants_size () << " participants";
      return;
    }

  LOG (INFO)
      << "Filing " << (isDispute ? "dispute" : "resolution")
      << " for channel " << id.ToHex () << " at height " << height;

  bool res;
  if (isDispute)
    res = ProcessDispute (*h, height, proof);
  else
    res = ProcessResolution (*h, proof);

  if (!res)
    LOG (WARNING) << "Dispute/resolution is invalid: " << obj;
}

void
ShipsLogic::ProcessExpiredDisputes (xaya::SQLiteDatabase& db,
                                    const unsigned height)
{
  LOG (INFO) << "Processing expired disputes for height " << height << "...";

  xaya::ChannelsTable tbl(db);
  auto stmt = tbl.QueryForDisputeHeight (height - DISPUTE_BLOCKS);
  while (stmt.Step ())
    {
      auto h = tbl.GetFromResult (stmt);
      const auto id = h->GetId ();
      const auto& meta = h->GetMetadata ();

      /* If there is a dispute filed on a channel, it means that we can
         make some assumptions on the channel already.  Mainly, that it has
         two participants, a valid state and is not in a no-turn state.  */
      CHECK_EQ (meta.participants_size (), 2);

      auto state = boardRules.ParseState (id, meta, h->GetLatestState ());
      CHECK (state != nullptr)
          << "Invalid on-chain state for disputed channel " << id.ToHex ()
          << ": " << h->GetLatestState ();
      const int loser = state->WhoseTurn ();
      CHECK_NE (loser, xaya::ParsedBoardState::NO_TURN);
      CHECK_GE (loser, 0);
      CHECK_LE (loser, 1);
      const int winner = 1 - loser;

      LOG (INFO)
          << "Dispute on channel " << id.ToHex ()
          << " expired, force-clsoing it now; "
          << meta.participants (winner).name () << " won, "
          << meta.participants (loser).name () << " lost";

      UpdateStats (db, meta, winner);
      h.reset ();
      tbl.DeleteById (id);
    }
}

void
ShipsLogic::UpdateStats (xaya::SQLiteDatabase& db,
                         const xaya::proto::ChannelMetadata& meta,
                         const int winner)
{
  CHECK_GE (winner, 0);
  CHECK_LE (winner, 1);
  CHECK_EQ (meta.participants_size (), 2);

  const int loser = 1 - winner;
  const std::string& winnerName = meta.participants (winner).name ();
  const std::string& loserName = meta.participants (loser).name ();

  auto stmt = db.Prepare (R"(
    INSERT OR IGNORE INTO `game_stats`
      (`name`, `won`, `lost`) VALUES (?1, 0, 0), (?2, 0, 0)
  )");
  stmt.Bind (1, winnerName);
  stmt.Bind (2, loserName);
  stmt.Execute ();

  stmt = db.Prepare (R"(
    UPDATE `game_stats`
      SET `won` = `won` + 1
      WHERE `name` = ?1
  )");
  stmt.Bind (1, winnerName);
  stmt.Execute ();

  stmt = db.Prepare (R"(
    UPDATE `game_stats`
      SET `lost` = `lost` + 1
      WHERE `name` = ?2
  )");
  stmt.Bind (2, loserName);
  stmt.Execute ();
}

void
ShipsLogic::UpdateState (xaya::SQLiteDatabase& db, const Json::Value& blockData)
{
  const auto& blk = blockData["block"];
  CHECK (blk.isObject ());
  const auto& heightVal = blk["height"];
  CHECK (heightVal.isUInt ());
  const unsigned height = heightVal.asUInt ();

  const auto& moves = blockData["moves"];
  CHECK (moves.isArray ());
  LOG (INFO) << "Processing " << moves.size () << " moves...";
  for (const auto& mv : moves)
    {
      CHECK (mv.isObject ()) << "Not an object: " << mv;

      const auto& nameVal = mv["name"];
      CHECK (nameVal.isString ());
      const std::string name = nameVal.asString ();

      const auto& txidVal = mv["txid"];
      CHECK (txidVal.isString ());
      xaya::uint256 txid;
      CHECK (txid.FromHex (txidVal.asString ()));

      const auto& data = mv["move"];
      if (!data.isObject ())
        {
          LOG (WARNING) << "Move by " << name << " is not an object: " << data;
          continue;
        }

      /* Some of the possible moves can interact with each other (e.g. joining
         a channel and filing a dispute immediately).  These interactions
         are not generally useful, and just complicate things (as we have to
         ensure that the order remains fixed and they keep working).  Thus
         let us simply forbid more than one action per move.  */
      if (data.size () > 1)
        {
          LOG (WARNING)
              << "Move by " << name << " has more than one action: " << data;
          continue;
        }

      HandleCreateChannel (db, data["c"], name, txid);
      HandleJoinChannel (db, data["j"], name, txid);
      HandleAbortChannel (db, data["a"], name);
      HandleCloseChannel (db, data["w"]);
      HandleDisputeResolution (db, data["d"], height, true);
      HandleDisputeResolution (db, data["r"], height, false);
    }

  ProcessExpiredDisputes (db, height);
}

Json::Value
ShipsLogic::GetStateAsJson (const xaya::SQLiteDatabase& db)
{
  GameStateJson gsj(db, boardRules);
  return gsj.GetFullJson ();
}

void
ShipsPending::HandleDisputeResolution (xaya::SQLiteDatabase& db,
                                       const Json::Value& obj)
{
  xaya::ChannelsTable tbl(db);
  xaya::proto::StateProof proof;
  auto h = ParseDisputeResolutionMove (obj, tbl, proof);
  if (h == nullptr)
    return;

  LOG (INFO) << "Obtained StateProof from pending move";
  VLOG (1) << "StateProof:\n" << proof.DebugString ();

  AddPendingStateProof (*h, proof);
}

void
ShipsPending::AddPendingMoveUnsafe (const xaya::SQLiteDatabase& db,
                                    const Json::Value& mv)
{
  CHECK (mv.isObject ()) << "Not an object: " << mv;

  const auto& data = mv["move"];
  if (!data.isObject ())
    {
      LOG (WARNING) << "Move is not an object: " << data;
      return;
    }

  /* We do not full validation here, only the things necessary for sane
     processing.  Even if a move is actually invalid, we can still apply
     its pending StateProof in case it is valid.  */

  auto& mutableDb = const_cast<xaya::SQLiteDatabase&> (db);
  HandleDisputeResolution (mutableDb, data["d"]);
  HandleDisputeResolution (mutableDb, data["r"]);
}

void
ShipsPending::AddPendingMove (const Json::Value& mv)
{
  AccessConfirmedState ();
  AddPendingMoveUnsafe (AccessConfirmedState (), mv);
}

} // namespace ships
