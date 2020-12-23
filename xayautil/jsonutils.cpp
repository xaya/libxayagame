// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "jsonutils.hpp"

#include <glog/logging.h>

#include <cmath>

namespace xaya
{

/** Number of satoshis in one full CHI.  */
constexpr int64_t COIN = 100'000'000;

/**
 * Upper bound for the maximum possible amount of CHI we consider valid.
 * This is not the exact total supply, just something we use to protect against
 * potential overflows.
 */
constexpr int64_t MAX_CHI_AMOUNT = 80'000'000 * COIN;

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

bool
ChiAmountFromJson (const Json::Value& val, int64_t& sat)
{
  if (!val.isDouble ())
    {
      LOG (ERROR) << "JSON value for amount is not double: " << val;
      return false;
    }
  const double dval = val.asDouble () * COIN;

  if (dval < 0.0 || dval > MAX_CHI_AMOUNT)
    {
      LOG (ERROR) << "Amount " << (dval / COIN) << " is out of range";
      return false;
    }

  sat = std::llround (dval);
  VLOG (1) << "Converted JSON " << val << " to amount: " << sat;

  /* Sanity check once more, to guard against potential overflow bugs.  */
  CHECK_GE (sat, 0);
  CHECK_LE (sat, MAX_CHI_AMOUNT);

  return true;
}

Json::Value
ChiAmountToJson (const int64_t sat)
{
  CHECK_GE (sat, 0);
  return Json::Value (static_cast<double> (sat) / COIN);
}

} // namespace xaya
