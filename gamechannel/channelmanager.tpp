// Copyright (C) 2019-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Template implementation code for channelmanager.hpp.  */

#include <glog/logging.h>

namespace xaya
{

template <typename State, typename Fcn>
  auto
  ChannelManager::ReadLatestState (const Fcn& cb) const
{
  std::lock_guard<std::mutex> lock(mut);

  if (!exists)
    return cb (nullptr);

  const auto& state = boardStates.GetLatestState ();
  const auto* typedState = dynamic_cast<const State*> (&state);
  CHECK (typedState != nullptr);
  return cb (typedState);
}

} // namespace xaya
