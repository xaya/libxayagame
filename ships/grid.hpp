// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_GRID_HPP
#define XAYASHIPS_GRID_HPP

#include "coord.hpp"

#include <cstdint>
#include <string>

namespace ships
{

/**
 * A bit vector with entries for every cell on the board.  Such a value
 * is used to represent the positions of ships, the hits and the already
 * guessed locations.
 */
class Grid
{

private:

  /** The underlying uint64, which we use as bit vector.  */
  uint64_t bits = 0;

  /* Verify that the size of our bit field matches the defined size of
     the board according to Coord.  */
  static_assert (sizeof (bits) * 8 == Coord::CELLS,
                 "Mismatch between Grid bit field and Coord::CELLS");

public:

  Grid () = default;

  explicit Grid (const uint64_t b)
    : bits(b)
  {}

  Grid (const Grid&) = default;
  Grid& operator= (const Grid&) = default;

  /**
   * Returns the raw bit vector value.  This is used for encoding it into
   * a protocol buffer.
   */
  uint64_t
  GetBits () const
  {
    return bits;
  }

  /**
   * Retrieves the bit for the given coordinate.
   */
  bool Get (const Coord& c) const;

  /**
   * Sets the bit at the given coordinate to true.  Must not be called if
   * the bit is already true.
   */
  void Set (const Coord& c);

  /**
   * Counts how many bits are true.
   */
  int CountOnes () const;

  /**
   * Returns the little-endian encoding of the bits as individual bytes.
   * This is used for hashing the value in a deterministic way.
   */
  std::string Blob () const;

  /**
   * Returns the number of cells covered by ships in a valid configuration.
   */
  static int TotalShipCells ();

};

/**
 * Verifies if the given grid of ship positions matches previous answers made
 * by a player to shots (based on a grid of where shots were made and which of
 * those were replied to as "hit").  The "hits" must be a subset of the
 * "targeted" positions.
 */
bool VerifyPositionForAnswers (const Grid& position, const Grid& targeted,
                               const Grid& hits);

/**
 * Verifies whether a given position of ships is valid with respect to the
 * number of ships and the placement rules.
 */
bool VerifyPositionOfShips (const Grid& position);

} // namespace ships

#endif // XAYASHIPS_GRID_HPP
