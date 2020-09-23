// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NONFUNGIBLE_MOVEPROCESSOR_HPP
#define NONFUNGIBLE_MOVEPROCESSOR_HPP

#include "assets.hpp"
#include "moveparser.hpp"

#include <xayagame/sqlitestorage.hpp>

#include <json/json.h>

#include <string>

namespace nf
{

/**
 * Processor for moves in confirmed blocks, i.e. which will be reflected
 * in an update to the game-state database.
 */
class MoveProcessor : private MoveParser
{

private:

  /**
   * Updates the balance of the given user with the given delta.
   */
  void UpdateBalance (const Asset& a, const std::string& name, Amount num);

protected:

  /**
   * Returns the underlying database instance as mutable.
   */
  xaya::SQLiteDatabase& MutableDb ();

  void ProcessMint (const Asset& a, Amount supply,
                    const std::string* data) override;

  void ProcessTransfer (const Asset& a, Amount num,
                        const std::string& sender,
                        const std::string& recipient) override;

  void ProcessBurn (const Asset& a, Amount num,
                    const std::string& sender) override;

public:

  explicit MoveProcessor (xaya::SQLiteDatabase& d)
    : MoveParser(d)
  {}

  MoveProcessor () = delete;
  MoveProcessor (const MoveProcessor&) = delete;
  void operator= (const MoveProcessor&) = delete;

  /**
   * Processes all moves from a given block (given as the block's
   * "moves" JSON array).
   */
  void ProcessAll (const Json::Value& moves);

};

} // namespace nf

#endif // NONFUNGIBLE_MOVEPROCESSOR_HPP
