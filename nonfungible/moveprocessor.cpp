// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "moveprocessor.hpp"

#include <glog/logging.h>

namespace nf
{

xaya::SQLiteDatabase&
MoveProcessor::MutableDb ()
{
  /* The constructor of MoveProcessor expects a mutable database, which is
     then just stored in a const& in MoveParser.  Thus it is fine to
     const-cast it back to be mutable here.  */
  return const_cast<xaya::SQLiteDatabase&> (db);
}

void
MoveProcessor::UpdateBalance (const Asset& a, const std::string& name,
                              const Amount num)
{
  const Amount oldBalance = GetBalance (a, name);
  const Amount newBalance = oldBalance + num;
  CHECK_GE (newBalance, 0);
  CHECK_LE (newBalance, MAX_AMOUNT);

  if (newBalance == 0)
    {
      auto stmt = MutableDb ().Prepare (R"(
        DELETE FROM `balances`
          WHERE `name` = ?1 AND `minter` = ?2 AND `asset` = ?3
      )");
      stmt.Bind (1, name);
      a.BindToParams (stmt, 2, 3);
      stmt.Execute ();
    }
  else
    {
      auto stmt = MutableDb ().Prepare (R"(
        INSERT OR REPLACE INTO `balances`
            (`name`, `minter`, `asset`, `balance`)
            VALUES (?1, ?2, ?3, ?4)
      )");
      stmt.Bind (1, name);
      a.BindToParams (stmt, 2, 3);
      stmt.Bind (4, newBalance);
      stmt.Execute ();
    }
}

void
MoveProcessor::ProcessMint (const Asset& a, const Amount supply,
                            const std::string* data)
{
  auto stmt = MutableDb ().Prepare (R"(
    INSERT INTO `assets`
      (`minter`, `asset`, `data`)
      VALUES (?1, ?2, ?3)
  )");
  a.BindToParams (stmt, 1, 2);
  if (data != nullptr)
    stmt.Bind (3, *data);
  else
    stmt.BindNull (3);
  stmt.Execute ();

  if (supply > 0)
    UpdateBalance (a, a.GetMinter (), supply);

  LOG (INFO) << "Minted " << supply << " of new asset " << a;
}

void
MoveProcessor::ProcessTransfer (const Asset& a, const Amount num,
                                const std::string& sender,
                                const std::string& recipient)
{
  UpdateBalance (a, sender, -num);
  UpdateBalance (a, recipient, num);
  LOG (INFO)
      << "Sent " << num << " of " << a
      << " from " << sender << " to " << recipient;
}

void
MoveProcessor::ProcessBurn (const Asset& a, const Amount num,
                            const std::string& sender)
{
  UpdateBalance (a, sender, -num);
  LOG (INFO) << sender << " burnt " << num << " of " << a;
}

void
MoveProcessor::ProcessAll (const Json::Value& moves)
{
  CHECK (moves.isArray ());
  LOG_IF (INFO, !moves.empty ())
      << "Processing " << moves.size () << " moves...";
  for (const auto& mv : moves)
    ProcessOne (mv);
}

} // namespace nf
