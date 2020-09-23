// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_PENDING_HPP
#define NONFUNGIBLE_PENDING_HPP

#include "assets.hpp"
#include "moveparser.hpp"

#include <xayagame/sqlitegame.hpp>
#include <xayagame/sqlitestorage.hpp>

#include <json/json.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

namespace nf
{

class NonFungibleLogic;

/**
 * A currently pending state.
 */
class PendingState
{

private:

  /**
   * All newly minted assets (that are pending).  The values are the associated
   * data strings or null if there is no data.
   */
  std::map<Asset, std::unique_ptr<std::string>> assets;

  /** Changes to any balances compared to the database.  */
  std::map<std::pair<std::string, Asset>, Amount> balances;

public:

  PendingState () = default;
  PendingState (PendingState&&) = default;
  PendingState& operator= (PendingState&&) = default;

  PendingState (const PendingState&) = delete;
  void operator= (const PendingState&) = delete;

  /**
   * Returns true if the given asset is in the list of newly minted ones.
   */
  bool IsNewAsset (const Asset& a) const;

  /**
   * Adds a new asset to the list of ones being minted.
   */
  void AddAsset (const Asset& a, const std::string* data);

  /**
   * Tries to look up a pending balance.  Returns true and sets the
   * output value if we have an entry, and returns false otherwise.
   */
  bool GetBalance (const Asset& a, const std::string& name,
                   Amount& balance) const;

  /**
   * Inserts or updates the pending balance.
   */
  void SetBalance (const Asset& a, const std::string& name, Amount balance);

  /**
   * Returns a JSON representation of the state.
   */
  Json::Value ToJson () const;

};

/**
 * MoveParser subclass that updates to a pending state (and takes it into
 * account for validation).
 */
class PendingStateUpdater : public MoveParser
{

private:

  /** The state being updated.  */
  PendingState& state;

  /**
   * Updates the balance of someone by a given amount.
   */
  void UpdateBalance (const Asset& a, const std::string& name, Amount num);

protected:

  void ProcessMint (const Asset& a, Amount supply,
                    const std::string* data) override;
  void ProcessTransfer (const Asset& a, Amount num,
                        const std::string& sender,
                        const std::string& recipient) override;
  void ProcessBurn (const Asset& a, Amount num,
                    const std::string& sender) override;

  bool AssetExists (const Asset& a) const override;
  Amount GetBalance (const Asset& a, const std::string& name) const override;

public:

  explicit PendingStateUpdater (const xaya::SQLiteDatabase& d, PendingState& s)
    : MoveParser(d), state(s)
  {}

};

/**
 * The tracker for pending moves, using the libxayagame framework.
 */
class PendingMoves : public xaya::SQLiteGame::PendingMoves
{

private:

  /** The current state of pending moves.  */
  PendingState state;

protected:

  void Clear () override;
  void AddPendingMove (const Json::Value& mv) override;

public:

  explicit PendingMoves (NonFungibleLogic& rules);

  Json::Value ToJson () const override;

};

} // namespace nf

#endif // NONFUNGIBLE_PENDING_HPP
