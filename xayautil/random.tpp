// Copyright (C) 2020 The Xaya developers
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
  Random::ShuffleN (Iterator begin, Iterator end, const size_t n)
{
  if (end - begin <= 1 || n == 0)
    return;

  const Iterator mid = begin + NextInt (end - begin);
  if (begin != mid)
    std::swap (*begin, *mid);

  ShuffleN (begin + 1, end, n - 1);
}

} // namespace xaya
