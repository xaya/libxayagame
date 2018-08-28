// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RPS_LOGIC_HPP
#define RPS_LOGIC_HPP

#include "xayagame/game.hpp"
#include "xayagame/storage.hpp"

#include <string>

namespace rps
{

/**
 * The actual implementation of the game rules.
 */
class RpsLogic : public xaya::GameLogic
{

public:

  void GetInitialState (const std::string& chain,
                        unsigned& height, std::string& hashHex,
                        xaya::GameStateData& state) override;

};

} // namespace rps

#endif // RPS_LOGIC_HPP
