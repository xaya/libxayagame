// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_UINT256_HPP
#define XAYAGAME_UINT256_HPP

#include <array>
#include <string>

namespace xaya
{

/**
 * A very basic type representing a constant uint256.  It can be compared
 * and converted to/from hex, but otherwise not manipulated.  This is used
 * to represent block hashes in the storage layer for Xaya games.
 */
class uint256 final
{

private:

  static constexpr size_t NUM_BYTES = 256 / 8;
  using Array = std::array<unsigned char, NUM_BYTES>;

  /** The raw bytes, stored as big-endian.  */
  Array data;

public:

  using const_iterator = Array::const_iterator;

  uint256 () = default;
  uint256 (const uint256&) = default;
  uint256 (uint256&&) = default;

  uint256& operator= (const uint256&) = default;
  uint256& operator= (uint256&&) = default;

  /**
   * Converts the uint256 to a lower-case, big-endian hex string.
   */
  std::string ToHex () const;

  /**
   * Parses a hex string as big-endian into this object.  Returns false if
   * the string is not valid (wrong size or invalid characters).
   */
  bool FromHex (const std::string& hex);

  const_iterator
  begin () const
  {
    return data.begin ();
  }

  const_iterator
  end () const
  {
    return data.end ();
  }

  friend bool
  operator== (const uint256& a, const uint256& b)
  {
    return a.data == b.data;
  }

  friend bool
  operator!= (const uint256& a, const uint256& b)
  {
    return !(a == b);
  }

  friend bool
  operator< (const uint256& a, const uint256& b)
  {
    return a.data < b.data;
  }

};

} // namespace xaya

#endif // XAYAGAME_UINT256_HPP
