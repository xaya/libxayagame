// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYASHIPS_COORD_HPP
#define XAYASHIPS_COORD_HPP

namespace ships
{

/**
 * Directions on the board.  The grid is laid out like a matrix.
 */
enum class Direction
{

  /** Decreasing column.  */
  LEFT,

  /** Increasing column.  */
  RIGHT,

  /** Decreasing row.  */
  UP,

  /** Decreasing column.  */
  DOWN,

};

/**
 * Returns the "inverse" direction.
 */
Direction operator- (Direction d);

/**
 * A coordinate on the 8x8 grid of the game.  This class can translate between
 * (r, c) coordinates and direct indices, and it can determine neighbours
 * and whether they are out of the board.
 */
class Coord
{

private:

  /**
   * The row in the range 0..7.  For coordinates that are outside of the board,
   * the value may be out of that range.
   */
  int row = 0;

  /** The column in the range 0..7.  Might be outside of the range.  */
  int column = 0;

public:

  /** The size of the board (side length).  */
  static constexpr int SIDE = 8;
  /** The size of the board in total number of cells.  */
  static constexpr int CELLS = SIDE * SIDE;

  Coord () = default;
  Coord (const Coord&) = default;
  Coord& operator= (const Coord&) = default;

  /**
   * Initialises an instance from the linearised index.
   */
  explicit Coord (int ind);

  /**
   * Initialises an instance from the (r, c) coordinates.
   */
  explicit Coord (const int r, const int c)
    : row(r), column(c)
  {}

  /**
   * Returns true if the coordinate is on the board.
   */
  bool IsOnBoard () const;

  /**
   * Returns the linearised index for this coordinate.  Must only be called
   * if the coordinate is on the board.
   */
  int GetIndex () const;

  /**
   * Changes the coordinate in the given direction.
   */
  Coord operator+ (Direction d) const;

  /**
   * Changes the coordinate in the inverse direction.
   */
  Coord operator- (Direction d) const;

  friend bool operator== (const Coord& a, const Coord& b);
  friend bool operator!= (const Coord& a, const Coord& b);

};

} // namespace ships

#endif // XAYASHIPS_COORD_HPP
