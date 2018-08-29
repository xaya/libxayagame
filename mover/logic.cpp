// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logic.hpp"

#include <glog/logging.h>

using xaya::GameStateData;

namespace mover
{

void
MoverLogic::GetInitialState (unsigned& height, std::string& hashHex,
                             GameStateData& state)
{
  /* In all cases, the initial game state is just empty.  */
  state = "";

  if (GetChain () == "main")
    {
      height = 125000;
      hashHex
          = "2aed5640a3be8a2f32cdea68c3d72d7196a7efbfe2cbace34435a3eef97561f2";
      return;
    }

  if (GetChain () == "test")
    {
      height = 10000;
      hashHex
          = "73d771be03c37872bc8ccd92b8acb8d7aa3ac0323195006fb3d3476784981a37";
      return;
    }

  if (GetChain () == "regtest")
    {
      height = 0;
      hashHex
          = "6f750b36d22f1dc3d0a6e483af45301022646dfc3b3ba2187865f5a7d6d83ab1";
      return;
    }

  LOG (FATAL) << "Unexpected chain: " << GetChain ();
}

} // namespace mover
