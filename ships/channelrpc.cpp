// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "channelrpc.hpp"

#include <glog/logging.h>

namespace ships
{

void
ShipsChannelRpcServer::stop ()
{
  LOG (INFO) << "RPC method called: stop";
  daemon.RequestStop ();
}

Json::Value
ShipsChannelRpcServer::getcurrentstate ()
{
  LOG (INFO) << "RPC method called: getcurrentstate";
  return ExtendStateJson (daemon.GetChannelManager ().ToJson ());
}

Json::Value
ShipsChannelRpcServer::waitforchange (const int knownVersion)
{
  LOG (INFO) << "RPC method called: waitforchange " << knownVersion;
  Json::Value state = daemon.GetChannelManager ().WaitForChange (knownVersion);
  return ExtendStateJson (std::move (state));
}

namespace
{

/**
 * Tries to parse a string into a position and validates it.  Returns true
 * and initialises grid if it is valid, and false otherwise.
 */
bool
ParseAndValidatePosition (const std::string& str, Grid& grid)
{
  if (!grid.FromString (str))
    {
      LOG (ERROR) << "Invalid position string given";
      return false;
    }

  if (!VerifyPositionOfShips (grid))
    {
      LOG (ERROR) << "Invalid ships position given";
      return false;
    }

  return true;
}

} // anonymous namespace

void
ShipsChannelRpcServer::setposition (const std::string& str)
{
  LOG (INFO) << "RPC method called: setposition\n" << str;

  if (channel.IsPositionSet ())
    {
      LOG (ERROR) << "Already set a position";
      return;
    }

  Grid pos;
  if (!ParseAndValidatePosition (str, pos))
    return;

  std::lock_guard<std::mutex> lock(mut);
  channel.SetPosition (pos);
  daemon.GetChannelManager ().TriggerAutoMoves ();
}

bool
ShipsChannelRpcServer::validateposition (const std::string& str)
{
  LOG (INFO) << "RPC method called: validateposition\n" << str;

  Grid pos;
  return ParseAndValidatePosition (str, pos);
}

void
ShipsChannelRpcServer::shoot (const int column, const int row)
{
  LOG (INFO) << "RPC method called: shoot " << row << " " << column;

  const Coord target(row, column);
  if (target.IsOnBoard ())
    {
      std::lock_guard<std::mutex> lock(mut);
      ProcessLocalMove (channel.GetShotMove (target));
    }
  else
    LOG (ERROR) << "Invalid coordinate given as shot target";
}

void
ShipsChannelRpcServer::revealposition ()
{
  LOG (INFO) << "RPC method called: revealposition";

  if (channel.IsPositionSet ())
    {
      std::lock_guard<std::mutex> lock(mut);
      ProcessLocalMove (channel.GetPositionRevealMove ());
    }
  else
    LOG (ERROR) << "Cannot reveal position if it is not set yet";
}

std::string
ShipsChannelRpcServer::filedispute ()
{
  LOG (INFO) << "RPC method called: filedispute";
  auto& cm = daemon.GetChannelManager ();

  /* If the winner is already known, we can't file an actual dispute, but
     instead we put the state on chain (with a resolution move) which will
     result in closure of the channel.  */
  bool hasWinner = false;
  cm.ReadLatestState<ShipsBoardState> (
      [&hasWinner] (const ShipsBoardState* state)
        {
          if (state != nullptr && state->GetState ().has_winner ())
            hasWinner = true;
        });

  const xaya::uint256 txid = hasWinner
      ? cm.PutStateOnChain ()
      : cm.FileDispute ();

  if (txid.IsNull ())
    return "";

  return txid.ToHex ();
}

Json::Value
ShipsChannelRpcServer::ExtendStateJson (Json::Value&& state) const
{
  std::lock_guard<std::mutex> lock(mut);

  if (channel.IsPositionSet ())
    state["myships"] = channel.GetPosition ().ToString ();

  return state;
}

void
ShipsChannelRpcServer::ProcessLocalMove (const proto::BoardMove& mv)
{
  xaya::BoardMove serialised;
  CHECK (mv.SerializeToString (&serialised));
  daemon.GetChannelManager ().ProcessLocalMove (serialised);
}

} // namespace ships
