// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "testutils.hpp"

#include <glog/logging.h>

#include <cstdio>
#include <string>

namespace xaya
{

uint256
BlockHash (unsigned num)
{
  std::string hex = "ab" + std::string (62, '0');

  CHECK (num < 0x100);
  std::sprintf (&hex[2], "%02x", num);
  CHECK (hex[4] == 0);
  hex[4] = '0';

  uint256 res;
  CHECK (res.FromHex (hex));
  return res;
}

} // namespace xaya
