# Copyright (C) 2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# All gamechannel files.  Shared between the native and WASM builds
# where noted.  Generated protobuf sources are handled separately via
# gamechannel_generate_protos() in proto.cmake.

# channelcore library sources (also used by the WASM build).
set (CHANNELCORE_SOURCES
  boardrules.cpp
  broadcast.cpp
  channelmanager.cpp
  channelstatejson.cpp
  ethsignatures.cpp
  movesender.cpp
  openchannel.cpp
  protoversion.cpp
  rollingstate.cpp
  signatures.cpp
  stateproof.cpp
)

# channelcore public headers.
set (CHANNELCORE_HEADERS
  boardrules.hpp
  broadcast.hpp
  channelmanager.hpp channelmanager.tpp
  channelstatejson.hpp
  ethsignatures.hpp
  movesender.hpp
  openchannel.hpp
  protoboard.hpp protoboard.tpp
  protoutils.hpp protoutils.tpp
  protoversion.hpp
  rollingstate.hpp
  signatures.hpp
  stateproof.hpp
)

# gamechannel library sources (native build only).
set (GAMECHANNEL_SOURCES
  channelgame.cpp
  chaintochannel.cpp
  daemon.cpp
  database.cpp
  gamestatejson.cpp
  gsprpc.cpp
  recvbroadcast.cpp
  rpcbroadcast.cpp
  rpcwallet.cpp
  syncmanager.cpp
)

# gamechannel public headers (native build only).
set (GAMECHANNEL_HEADERS
  channelgame.hpp
  chaintochannel.hpp
  daemon.hpp
  database.hpp
  gamestatejson.hpp
  gsprpc.hpp
  recvbroadcast.hpp
  rpcbroadcast.hpp
  rpcwallet.hpp
  schema.hpp
  syncmanager.hpp
)

# Unit test sources (native build only).
set (GAMECHANNEL_TEST_SOURCES
  broadcast_tests.cpp
  channelgame_tests.cpp
  channelmanager_tests.cpp
  channelstatejson_tests.cpp
  chaintochannel_tests.cpp
  database_tests.cpp
  ethsignatures_tests.cpp
  gamestatejson_tests.cpp
  movesender_tests.cpp
  protoboard_tests.cpp
  protoutils_tests.cpp
  protoversion_tests.cpp
  recvbroadcast_tests.cpp
  rollingstate_tests.cpp
  schema_tests.cpp
  signatures_tests.cpp
  stateproof_tests.cpp
  syncmanager_tests.cpp
  testgame_tests.cpp
  testgame.cpp
)
