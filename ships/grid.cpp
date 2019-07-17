// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "grid.hpp"

#include <glog/logging.h>

#include <map>
#include <sstream>

namespace ships
{

namespace
{

/**
 * An entry for the configuration of available ships.
 */
struct AvailableShipType
{
  unsigned size;
  unsigned number;
};

/** The ships that should be placed for a valid position.  */
constexpr AvailableShipType AVAILABLE_SHIPS[] =
  {
    {2, 4},
    {3, 2},
    {4, 1},
  };

} // anonymous namespace

std::string
Grid::ToString () const
{
  std::ostringstream res;
  for (int r = 0; r < Coord::SIDE; ++r)
    {
      for (int c = 0; c < Coord::SIDE; ++c)
        res << (Get (Coord (r, c)) ? 'x' : '.');
      res << '\n';
    }

  return res.str ();
}

bool
Grid::FromString (const std::string& str)
{
  bits = 0;

  int next = 0;
  for (const char c : str)
    {
      if (c == ' ' || c == '\n')
        continue;

      if (next >= Coord::CELLS)
        {
          LOG (ERROR) << "Too much data in string for a grid:\n" << str;
          return false;
        }

      switch (c)
        {
        case '.':
          break;
        case 'x':
          Set (Coord (next));
          break;
        default:
          LOG (ERROR) << "Invalid character in grid string: " << c;
          return false;
        }

      ++next;
    }

  if (next < Coord::CELLS)
    {
      LOG (ERROR) << "Too few data in string for a grid:\n" << str;
      return false;
    }
  CHECK_EQ (next, Coord::CELLS);

  return true;
}

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

int
Grid::TotalShipCells ()
{
  int res = 0;
  for (const auto& t : AVAILABLE_SHIPS)
    res += t.size * t.number;

  return res;
}

bool
VerifyPositionForAnswers (const Grid& position, const Grid& targeted,
                          const Grid& hits)
{
  CHECK_EQ (hits.GetBits () & targeted.GetBits (), hits.GetBits ())
      << "Hit positions are not a subset of targeted positions";
  return (position.GetBits () & targeted.GetBits ()) == hits.GetBits ();
}

namespace
{

/**
 * Checks if the given coordinate has a ship on the board.  This verifies
 * that it is not out of board and if so, that there is a bit set.
 */
bool
HasShip (const Grid& g, const Coord& c)
{
  if (!c.IsOnBoard ())
    return false;

  return g.Get (c);
}

/**
 * Given a starting coordinate (top/left most of a ship) and the direction
 * of the ship (as well as the orthogonal direction), follow it until the
 * end and figure out how large it is.
 *
 * This also verifies that the placement is valid, which means that all
 * neighbour coordinates must be free (or out-of-board).
 *
 * Returns true if the placement is valid.  In that case, size will be
 * set to the size of the ship.
 */
bool
CheckShip (const Grid& g, Coord c,
           const Direction dir, const Direction otherDir,
           unsigned& size)
{
  CHECK (HasShip (g, c));

  /* Check that there are no other ships at the "head side" of it.  */
  if (HasShip (g, c - dir)
        || HasShip (g, c - dir - otherDir)
        || HasShip (g, c - dir + otherDir))
    {
      LOG (WARNING) << "There is another ship at the 'head side'";
      return false;
    }

  /* Traverse along the ship and check that there are no other ships next
     to the current tile.  */
  size = 0;
  for (; HasShip (g, c); c = c + dir)
    {
      ++size;
      if (HasShip (g, c - otherDir) || HasShip (g, c + otherDir))
        {
          LOG (WARNING) << "There is another ship next to it";
          return false;
        }
    }

  /* Finally, verify that there is no other ship at the "tail side".  */
  if (HasShip (g, c - otherDir) || HasShip (g, c + otherDir))
    {
      LOG (WARNING) << "There is another ship at the 'tail side'";
      return false;
    }

  return true;
}

} // anonymous namespace

bool
VerifyPositionOfShips (const Grid& position)
{
  std::map<unsigned, unsigned> foundShips;
  for (int i = 0; i < Coord::CELLS; ++i)
    {
      const Coord c(i);
      if (!position.Get (c))
        continue;

      /* If there is a ship also to the left or above, then we ignore this
         for now as well.  Those tiles are processed when walking that ship,
         starting from the top-most / left-most tile.  */
      if (HasShip (position, c + Direction::UP)
            || HasShip (position, c + Direction::LEFT))
        continue;

      /* Try whether this ship is horizontal or vertical.  */
      Direction dir;
      Direction otherDir;
      if (HasShip (position, c + Direction::DOWN))
        {
          dir = Direction::DOWN;
          otherDir = Direction::RIGHT;
        }
      else
        {
          /* Here, we do not check whether there really is another ship to
             the right.  If there is not, then this will simply be seen
             as a size-one ship.  */
          dir = Direction::RIGHT;
          otherDir = Direction::DOWN;
        }

      unsigned size;
      if (!CheckShip (position, c, dir, otherDir, size))
        return false;

      ++foundShips[size];
    }

  /* Finally, verify the number of each type of ship.  */
  unsigned availableTypes = 0;
  for (const auto& t : AVAILABLE_SHIPS)
    {
      ++availableTypes;

      const auto mit = foundShips.find (t.size);
      if (mit == foundShips.end ())
        {
          LOG (WARNING) << "Found no ships of size " << t.size;
          return false;
        }

      if (mit->second != t.number)
        {
          LOG (WARNING)
              << "Found " << mit->second << " ships of size " << t.size
              << ", expected " << t.number;
          return false;
        }
    }
  if (availableTypes != foundShips.size ())
    {
      LOG (WARNING)
          << "Found " << foundShips.size ()
          << " types of ships, expected " << availableTypes;
      return false;
    }

  return true;
}

} // namespace ships
