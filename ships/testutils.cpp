// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include "coord.hpp"

#include <glog/logging.h>

namespace ships
{

Grid
GridFromString (const std::string& str)
{
  CHECK_EQ (str.size (), Coord::CELLS);

  Grid g;
  for (int i = 0; i < Coord::CELLS; ++i)
    switch (str[i])
      {
      case '.':
        break;
      case 'x':
        g.Set (Coord (i));
        break;
      default:
        LOG (FATAL) << "Invalid character in position string: " << str[i];
      }

  return g;
}

} // namespace ships
