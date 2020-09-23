// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "jsonutils.hpp"

namespace xaya
{

bool
IsIntegerValue (const Json::Value& val)
{
  switch (val.type ())
    {
    case Json::intValue:
    case Json::uintValue:
      return true;

    default:
      return false;
    }
}

} // namespace xaya
