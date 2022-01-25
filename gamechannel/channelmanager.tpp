// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Template implementation code for channelmanager.hpp.  */

#include <glog/logging.h>

namespace xaya
{

template <typename State>
  const State*
  ChannelManager::GetBoardState () const
{
  if (!exists)
    return nullptr;

  const auto& state = boardStates.GetLatestState ();
  const auto* typedState = dynamic_cast<const State*> (&state);
  CHECK (typedState != nullptr);
  return typedState;
}

} // namespace xaya
