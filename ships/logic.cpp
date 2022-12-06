// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include "gamestatejson.hpp"
#include "schema.hpp"

#include <gamechannel/database.hpp>
#include <gamechannel/proto/stateproof.pb.h>
#include <gamechannel/protoutils.hpp>
#include <xayautil/base64.hpp>

#include <glog/logging.h>

namespace ships
{

/* ************************************************************************** */

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
      height = 2'960'000;
      hashHex
          = "81c60638621eec528667941d954e044577f0125465ca2ba26347385d5e3aecdd";
      break;

    case xaya::Chain::TEST:
      height = 112'307;
      hashHex
          = "4a2497b5ce649747f9dffeab6fafd57aa928901f3b15537287359adf5ed6fb1a";
      break;

    case xaya::Chain::REGTEST:
      height = 0;
      hashHex
          = "6f750b36d22f1dc3d0a6e483af45301022646dfc3b3ba2187865f5a7d6d83ab1";
      break;

    case xaya::Chain::POLYGON:
      height = 36'500'000;
      hashHex
          = "7330add6385f12fa3050e9abe040900283df0b5c8f94003bf713ac9cc8cf0442";
      break;

    case xaya::Chain::MUMBAI:
      height = 29'520'000;
      hashHex
        = "ca20c39d7e9de4a46319327154eec865cdc02990c7bf17a3543ac86c5499cb58";
      break;

    case xaya::Chain::GANACHE:
      height = 0;
      /* Ganache does not have a fixed genesis block.  So leave the block
         hash open and just accept any at height 0.  */
      hashHex = "";
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

/* ************************************************************************** */

namespace
{

/**
 * Tries to parse a "create channel" move.  If the move is valid, the signing
 * address is set.
 */
bool
ParseCreateChannelMove (const Json::Value& obj, std::string& addr)
{
  if (!obj.isObject ())
    return false;

  const auto& addrVal = obj["addr"];
  if (obj.size () != 1 || !addrVal.isString ())
    {
      LOG (WARNING) << "Invalid create channel move: " << obj;
      return false;
    }
  addr = addrVal.asString ();

  return true;
}

/**
 * Tries to process a "create channel" move, if the JSON object describes
 * a valid one.
 */
void
HandleCreateChannel (xaya::SQLiteDatabase& db,
                     const Json::Value& obj, const unsigned height,
                     const std::string& name, const xaya::uint256& id)
{
  std::string addr;
  if (!ParseCreateChannelMove (obj, addr))
    return;

  LOG (INFO)
      << "Creating channel with ID " << id.ToHex ()
      << " for user " << name << " with address " << addr;

  xaya::ChannelsTable tbl(db);

  /* Verify that this is indeed a new instance and not an existing one.  That
     should never happen, assuming that id's do not collide (which must
     be guaranteed by choosing a proper source of id's).  */
  auto h = tbl.GetById (id);
  CHECK (h == nullptr) << "Already have channel with ID " << id.ToHex ();

  h = tbl.CreateNew (id);
  xaya::proto::ChannelMetadata meta;
  auto* p = meta.add_participants ();
  p->set_name (name);
  p->set_address (addr);
  h->Reinitialise (meta, "");

  auto stmt = db.Prepare (R"(
    INSERT INTO `channel_extradata`
      (`id`, `createdheight`, `participants`)
      VALUES (?1, ?2, 1)
  )");
  stmt.Bind (1, h->GetId ());
  stmt.Bind (2, height);
  stmt.Execute ();
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
 * Tries to parse and validate a "join channel" move.  If the move seems
 * valid, the handle is returned and the second player's signing address
 * is set.  Returns null if the move is not valid.
 */
xaya::ChannelsTable::Handle
ParseJoinChannelMove (const Json::Value& obj, const std::string& name,
                      xaya::ChannelsTable& tbl, std::string& addr)
{
  if (!obj.isObject ())
    return nullptr;

  const auto& addrVal = obj["addr"];
  if (obj.size () != 2 || !addrVal.isString ())
    {
      LOG (WARNING) << "Invalid join channel move: " << obj;
      return nullptr;
    }
  addr = addrVal.asString ();

  auto h = RetrieveChannelFromMove (obj, tbl);
  if (h == nullptr)
    return nullptr;

  const auto& meta = h->GetMetadata ();
  if (meta.participants_size () != 1)
    {
      LOG (WARNING)
          << "Cannot join channel " << h->GetId ().ToHex ()
          << " with " << meta.participants_size () << " participants";
      return nullptr;
    }

  if (meta.participants (0).name () == name)
    {
      LOG (WARNING)
          << name << " cannot join channel " << h->GetId ().ToHex ()
          << " a second time";
      return nullptr;
    }

  return h;
}

/**
 * Tries to process a "join channel" move.
 */
void
HandleJoinChannel (xaya::SQLiteDatabase& db,
                   const Json::Value& obj, const std::string& name,
                   const xaya::uint256& id)
{
  xaya::ChannelsTable tbl(db);

  std::string addr;
  auto h = ParseJoinChannelMove (obj, name, tbl, addr);
  if (h == nullptr)
    return;

  LOG (INFO)
      << "Adding " << name << " to channel " << h->GetId ().ToHex ()
      << " with address " << addr;

  xaya::proto::ChannelMetadata newMeta = h->GetMetadata ();
  xaya::UpdateMetadataReinit (id, newMeta);
  auto* p = newMeta.add_participants ();
  p->set_name (name);
  p->set_address (addr);
  CHECK_EQ (newMeta.participants_size (), 2);

  xaya::BoardState state;
  CHECK (InitialBoardState ().SerializeToString (&state));
  h->Reinitialise (newMeta, state);

  auto stmt = db.Prepare (R"(
    UPDATE `channel_extradata`
      SET `participants` = ?2
      WHERE `id` = ?1
  )");
  stmt.Bind (1, h->GetId ());
  stmt.Bind (2, newMeta.participants_size ());
  stmt.Execute ();
}

/**
 * Tries to parse and validate an "abort channel" move.  If the move seems
 * valid, the ID of the channel to abort is set.
 */
bool
ParseAbortChannelMove (const Json::Value& obj, const std::string& name,
                       xaya::ChannelsTable& tbl, xaya::uint256& id)
{
  if (!obj.isObject ())
    return false;

  if (obj.size () != 1)
    {
      LOG (WARNING) << "Invalid abort channel move: " << obj;
      return false;
    }

  auto h = RetrieveChannelFromMove (obj, tbl);
  if (h == nullptr)
    return false;

  id = h->GetId ();
  const auto& meta = h->GetMetadata ();
  if (meta.participants_size () != 1)
    {
      LOG (WARNING)
          << "Cannot abort channel " << id.ToHex ()
          << " with " << meta.participants_size () << " participants";
      return false;
    }

  if (meta.participants (0).name () != name)
    {
      LOG (WARNING)
          << name << " cannot abort channel " << id.ToHex ()
          << ", only " << meta.participants (0).name () << " can";
      return false;
    }

  return true;
}

/**
 * Deletes a channel from the database by ID.  This deletes it from the
 * game-channel library managed table, as well as from our extra-data one.
 */
void
DeleteChannelById (xaya::SQLiteDatabase& db, xaya::ChannelsTable& tbl,
                   const xaya::uint256& id)
{
  tbl.DeleteById (id);

  auto stmt = db.Prepare (R"(
    DELETE FROM `channel_extradata`
      WHERE `id` = ?1
  )");
  stmt.Bind (1, id);
  stmt.Execute ();
}

/**
 * Tries to process an "abort channel" move.
 */
void
HandleAbortChannel (xaya::SQLiteDatabase& db,
                    const Json::Value& obj, const std::string& name)
{
  xaya::ChannelsTable tbl(db);

  xaya::uint256 id;
  if (!ParseAbortChannelMove (obj, name, tbl, id))
    return;

  LOG (INFO) << "Aborting channel " << id.ToHex ();
  DeleteChannelById (db, tbl, id);
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
ShipsLogic::HandleDeclareLoss (xaya::SQLiteDatabase& db,
                               const Json::Value& obj, const std::string& name)
{
  if (!obj.isObject ())
    return;

  if (obj.size () != 2)
    {
      LOG (WARNING) << "Invalid declare loss move: " << obj;
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
          << "Cannot declare loss in channel " << id.ToHex ()
          << " with " << meta.participants_size () << " participants";
      return;
    }

  const auto reinitVal = obj["r"];
  std::string reinit;
  if (!reinitVal.isString ()
        || !xaya::DecodeBase64 (reinitVal.asString (), reinit))
    {
      LOG (WARNING) << "Invalid reinit value on declare loss: " << obj;
      return;
    }
  if (reinit != meta.reinit ())
    {
      LOG (WARNING)
          << "Loss declaration is for different reinit than the channel: "
          << obj;
      return;
    }

  int loser = -1;
  for (int i = 0; i < meta.participants_size (); ++i)
    if (meta.participants (i).name () == name)
      {
        CHECK_EQ (loser, -1)
            << name << " participates multiple times in channel "
            << id.ToHex ();
        loser = i;
      }
  if (loser == -1)
    {
      LOG (WARNING)
          << name << " cannot declare loss on " << id.ToHex ()
          << " as non-participant";
      return;
    }
  CHECK_EQ (meta.participants (loser).name (), name);

  CHECK_GE (loser, 0);
  CHECK_LE (loser, 1);
  const int winner = 1 - loser;

  LOG (INFO)
      << name << " declared loss on channel " << id.ToHex ()
      << ", " << meta.participants (winner).name () << " is the winner";

  UpdateStats (db, meta, winner);
  h.reset ();
  DeleteChannelById (db, tbl, id);
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
    {
      LOG (WARNING) << "Dispute/resolution is invalid: " << obj;
      return;
    }

  /* If the on-chain state has a determined winner, close the channel
     right away accordingly.  This makes it possible for the winner
     to force-close the channel (through filing a resolution) even if the
     loser does not declare their loss.  */
  CHECK_EQ (meta.participants_size (), 2);
  auto baseState = boardRules.ParseState (id, meta, h->GetLatestState ());
  CHECK (baseState != nullptr)
      << "Invalid on-chain state for channel " << id.ToHex ()
      << ": " << h->GetLatestState ();
  const auto* state = dynamic_cast<const ShipsBoardState*> (baseState.get ());
  CHECK (state != nullptr);
  const auto& pb = state->GetState ();
  if (pb.has_winner ())
    {
      CHECK_GE (pb.winner (), 0);
      CHECK_LE (pb.winner (), 1);
      LOG (INFO)
          << "On-chain state of channel " << id.ToHex ()
          << " has winner " << meta.participants (pb.winner ()).name ()
          << ", closing now";

      UpdateStats (db, meta, pb.winner ());
      h.reset ();
      DeleteChannelById (db, tbl, id);
    }
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
      DeleteChannelById (db, tbl, id);
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

namespace
{

/**
 * Auto-closes all channels that have just one participant and been open
 * for a timeout number of blocks.
 */
void
TimeOutChannels (xaya::SQLiteDatabase& db, const unsigned height)
{
  /* Make sure we don't underflow for the first couple of blocks, particularly
     on regtest.  */
  if (height < CHANNEL_TIMEOUT_BLOCKS)
    return;

  auto stmt = db.PrepareRo (R"(
    SELECT `id`, `createdheight`, `participants`
      FROM `channel_extradata`
      WHERE `participants` < 2 AND `createdheight` <= ?1
  )");
  stmt.Bind (1, height - CHANNEL_TIMEOUT_BLOCKS);

  unsigned num = 0;
  xaya::ChannelsTable tbl(db);
  while (stmt.Step ())
    {
      const auto id = stmt.Get<xaya::uint256> (0);
      CHECK_EQ (stmt.Get<int> (1), height - CHANNEL_TIMEOUT_BLOCKS);
      CHECK_EQ (stmt.Get<int> (2), 1);
      DeleteChannelById (db, tbl, id);
      ++num;
    }

  LOG_IF (INFO, num > 0)
      << "Timed out " << num << " channels at height " << height;
}

/**
 * Extracts the move ID from a JSON representation.  These are used as
 * channel IDs for created channels, and to update the reinit on joins.
 * When an explicit "mvid" field is available (e.g. on Xaya-X-on-Eth),
 * it is used; otherwise we fall back to the txid (e.g. on Xaya Core).
 */
xaya::uint256
GetIdFromMove (const Json::Value& mv)
{
  CHECK (mv.isObject ());

  const Json::Value* field;
  if (mv.isMember ("mvid"))
    field = &mv["mvid"];
  else
    field = &mv["txid"];

  CHECK (field->isString ());
  xaya::uint256 id;
  CHECK (id.FromHex (field->asString ()));

  return id;
}

} // anonymous namespace

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

      const auto id = GetIdFromMove (mv);

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

      HandleCreateChannel (db, data["c"], height, name, id);
      HandleJoinChannel (db, data["j"], name, id);
      HandleAbortChannel (db, data["a"], name);
      HandleDeclareLoss (db, data["l"], name);
      HandleDisputeResolution (db, data["d"], height, true);
      HandleDisputeResolution (db, data["r"], height, false);
    }

  ProcessExpiredDisputes (db, height);
  TimeOutChannels (db, height);
}

Json::Value
ShipsLogic::GetStateAsJson (const xaya::SQLiteDatabase& db)
{
  GameStateJson gsj(db, boardRules);
  return gsj.GetFullJson ();
}

/* ************************************************************************** */

void
ShipsPending::ClearShips ()
{
  create = Json::Value (Json::arrayValue);
  join = Json::Value (Json::arrayValue);
  abort.clear ();
}

void
ShipsPending::HandleCreateChannel (const Json::Value& obj,
                                   const std::string& name,
                                   const xaya::uint256& id)
{
  std::string addr;
  if (!ParseCreateChannelMove (obj, addr))
    return;

  LOG (INFO)
      << "New pending create-channel move from " << name
      << ": " << id.ToHex ();

  Json::Value cur(Json::objectValue);
  cur["name"] = name;
  cur["address"] = addr;
  cur["id"] = id.ToHex ();
  create.append (cur);
}

void
ShipsPending::HandleJoinChannel (xaya::SQLiteDatabase& db,
                                 const Json::Value& obj,
                                 const std::string& name)
{
  xaya::ChannelsTable tbl(db);

  std::string addr;
  auto h = ParseJoinChannelMove (obj, name, tbl, addr);
  if (h == nullptr)
    return;

  LOG (INFO)
      << "New pending join-channel move from " << name
      << " for channel " << h->GetId ().ToHex ()
      << " with address " << addr;

  Json::Value cur(Json::objectValue);
  cur["name"] = name;
  cur["address"] = addr;
  cur["id"] = h->GetId ().ToHex ();
  join.append (cur);
}

void
ShipsPending::HandleAbortChannel (xaya::SQLiteDatabase& db,
                                  const Json::Value& obj,
                                  const std::string& name)
{
  xaya::ChannelsTable tbl(db);

  xaya::uint256 id;
  if (!ParseAbortChannelMove (obj, name, tbl, id))
    return;

  LOG (INFO)
      << "New pending abort-channel move from " << name
      << " for channel " << id.ToHex ();

  abort.insert (id);
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

  const auto& nameVal = mv["name"];
  CHECK (nameVal.isString ());
  const std::string name = nameVal.asString ();

  const auto id = GetIdFromMove (mv);

  const auto& data = mv["move"];
  if (!data.isObject ())
    {
      LOG (WARNING)
          << "Pending move by " << name
          << " is not an object: " << data;
      return;
    }
  if (data.size () > 1)
    {
      LOG (WARNING)
          << "Pending move by " << name
          << " has more than one action: " << data;
      return;
    }

  /* We do not full validation here, only the things necessary for sane
     processing.  Even if a move is actually invalid, we can still apply
     its pending StateProof in case it is valid.  */

  auto& mutableDb = const_cast<xaya::SQLiteDatabase&> (db);
  HandleCreateChannel (data["c"], name, id);
  HandleJoinChannel (mutableDb, data["j"], name);
  HandleAbortChannel (mutableDb, data["a"], name);
  HandleDisputeResolution (mutableDb, data["d"]);
  HandleDisputeResolution (mutableDb, data["r"]);
}

void
ShipsPending::Clear ()
{
  PendingMoves::Clear ();
  ClearShips ();
}

void
ShipsPending::AddPendingMove (const Json::Value& mv)
{
  AddPendingMoveUnsafe (AccessConfirmedState (), mv);
}

Json::Value
ShipsPending::ToJson () const
{
  Json::Value res = PendingMoves::ToJson ();
  res["create"] = create;
  res["join"] = join;

  Json::Value abortJson(Json::arrayValue);
  for (const auto& id : abort)
    abortJson.append (id.ToHex ());
  res["abort"] = abortJson;

  return res;
}

/* ************************************************************************** */

} // namespace ships
