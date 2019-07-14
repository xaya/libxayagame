// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "openchannel.hpp"

#include "movesender.hpp"

namespace xaya
{

bool
OpenChannel::MaybeAutoMove (const ParsedBoardState& state, BoardMove& mv)
{
  return false;
}

void
OpenChannel::MaybeOnChainMove (const proto::ChannelMetadata& meta,
                               const ParsedBoardState& state,
                               MoveSender& sender)
{}

} // namespace xaya
