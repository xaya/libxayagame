// Copyright (C) 2020-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Template code for random.hpp.  */

#include <algorithm>

namespace xaya
{

template <typename Iterator>
  void
  Random::Shuffle (Iterator begin, Iterator end)
{
  ShuffleN (begin, end, end - begin);
}

template <typename Iterator>
  void
  Random::ShuffleN (Iterator begin, Iterator end, size_t n)
{
  while (end - begin > 1 && n > 0)
    {
      const Iterator mid = begin + NextInt (end - begin);
      if (begin != mid)
        std::swap (*begin, *mid);

      ++begin;
      --n;
    }
}

} // namespace xaya
