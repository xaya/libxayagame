// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets.hpp"

#include "dbutils.hpp"

namespace nf
{

Json::Value
AmountToJson (const Amount n)
{
  return static_cast<Json::Int64> (n);
}

bool
AmountFromJson (const Json::Value& val, Amount& n)
{
  if (!val.isInt64 ())
    return false;

  n = val.asInt64 ();
  return n >= 0 && n <= MAX_AMOUNT;
}

void
Asset::BindToParams (sqlite3_stmt* stmt,
                     const int indMinter, const int indName) const
{
  BindParam (stmt, indMinter, minter);
  BindParam (stmt, indName, name);
}

Json::Value
Asset::ToJson () const
{
  Json::Value res(Json::objectValue);
  res["m"] = minter;
  res["a"] = name;
  return res;
}

Asset
Asset::FromColumns (sqlite3_stmt* stmt, const int indMinter, const int indName)
{
  return Asset (ColumnExtract<std::string> (stmt, indMinter),
                ColumnExtract<std::string> (stmt, indName));
}

bool
Asset::IsValidName (const std::string& nm)
{
  for (const unsigned char c : nm)
    if (c < 0x20)
      return false;

  return true;
}

namespace
{

/**
 * Returns true and extracts the result as string if the given JSON value
 * is a string and does not contain any non-printable characters.
 */
bool
GetPrintableString (const Json::Value& val, std::string& res)
{
  if (!val.isString ())
    return false;

  res = val.asString ();
  return Asset::IsValidName (res);
}

} // anonymous namespace

bool
Asset::FromJson (const Json::Value& val)
{
  if (!val.isObject () || val.size () != 2)
    return false;

  if (!GetPrintableString (val["m"], minter))
    return false;
  if (!GetPrintableString (val["a"], name))
    return false;

  return true;
}

std::ostream&
operator<< (std::ostream& out, const Asset& a)
{
  out << a.minter << "/" << a.name;
  return out;
}

} // namespace nf
