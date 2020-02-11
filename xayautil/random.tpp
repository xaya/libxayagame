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
  if (begin == end)
    return;

  for (Iterator i = begin; i + 1 != end; ++i)
    {
      const Iterator j = i + NextInt (end - i);
      if (i != j)
        std::swap (*i, *j);
    }
}

} // namespace xaya
