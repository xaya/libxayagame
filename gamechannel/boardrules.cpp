// Copyright (C) 2019-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "boardrules.hpp"

namespace xaya
{

constexpr int ParsedBoardState::NO_TURN;

std::set<int>
ParsedBoardState::RequiredSignatures () const
{
  std::set<int> res;
  for (int i = 0; i < meta.participants_size (); ++i)
    res.insert (i);
  return res;
}

Json::Value
ParsedBoardState::ToJson () const
{
  return Json::Value ();
}

} // namespace xaya
