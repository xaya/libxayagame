// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "grid.hpp"

#include <glog/logging.h>

namespace ships
{

bool
Grid::Get (const Coord& c) const
{
  return (bits >> c.GetIndex ()) % 2 > 0;
}

void
Grid::Set (const Coord& c)
{
  const uint64_t mask = (static_cast<uint64_t> (1) << c.GetIndex ());
  CHECK_EQ (bits & mask, 0);
  bits |= mask;
}

int
Grid::CountOnes () const
{
  uint64_t remaining = bits;

  int res = 0;
  while (remaining > 0)
    {
      res += remaining % 2;
      remaining >>= 1;
    }

  return res;
}

std::string
Grid::Blob () const
{
  uint64_t remaining = bits;

  std::string res(8, '\0');
  for (size_t i = 0; i < res.size (); ++i)
    {
      res[i] = static_cast<char> (remaining & 0xFF);
      remaining >>= 8;
    }

  CHECK_EQ (remaining, 0);

  return res;
}

} // namespace ships
