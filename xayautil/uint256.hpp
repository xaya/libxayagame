// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_UINT256_HPP
#define XAYAUTIL_UINT256_HPP

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

public:

  static constexpr size_t NUM_BYTES = 256 / 8;

private:

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

  /**
   * Returns a pointer to the data blob that holds the raw binary data.
   * Its length is NUM_BYTES.
   */
  const unsigned char*
  GetBlob () const
  {
    return data.data ();
  }

  /**
   * Sets the data from a raw blob of bytes, which must be of length NUM_BYTES.
   */
  void FromBlob (const unsigned char* blob);

  /**
   * Checks if this number is all-zeros, which is used as "null" value.
   */
  bool IsNull () const;

  /**
   * Sets the value to all-zeros, corresponding to a "null" value (since this
   * is a hash that will never occur in practice).
   */
  void SetNull ();

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

#endif // XAYAUTIL_UINT256_HPP
