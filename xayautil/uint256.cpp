// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uint256.hpp"

#include <glog/logging.h>

#include <algorithm>
#include <cstdio>

namespace xaya
{

std::string
uint256::ToHex () const
{
  std::string result(NUM_BYTES * 2, 'x');
  for (size_t i = 0; i < NUM_BYTES; ++i)
    std::sprintf (&result[2 * i], "%02x", static_cast<uint8_t> (data[i]));
  return result;
}

namespace
{

bool
ParseHexDigit (const char digit, uint_fast8_t& target)
{
  if (digit >= '0' && digit <= '9')
    {
      target = (digit - '0');
      return true;
    }

  if (digit >= 'a' && digit <= 'f')
    {
      target = 0xA + (digit - 'a');
      return true;
    }
  if (digit >= 'A' && digit <= 'F')
    {
      target = 0xA + (digit - 'A');
      return true;
    }

  LOG (ERROR) << "Invalid hex digit: '" << digit << "'";
  return false;
}

} // anonymous namespace

bool
uint256::FromHex (const std::string& hex)
{
  if (hex.size () != NUM_BYTES * 2)
    {
      LOG (ERROR) << "Invalid-sized string for uint256: " << hex;
      return false;
    }

  Array newData;
  for (size_t i = 0; i < NUM_BYTES; ++i)
    {
      uint_fast8_t hi, lo;
      if (!ParseHexDigit (hex[2 * i], hi)
            || !ParseHexDigit (hex[2 * i + 1], lo))
        return false;
      newData[i] = (hi << 4) | lo;
    }

  data = std::move (newData);
  return true;
}

void
uint256::FromBlob (const unsigned char* blob)
{
  std::copy (blob, blob + NUM_BYTES, data.data ());
}

bool
uint256::IsNull () const
{
  return std::all_of (data.begin (), data.end (),
                      [] (const Array::value_type val) { return val == 0; });
}

void
uint256::SetNull ()
{
  std::fill (data.begin (), data.end (), 0);
}

} // namespace xaya
