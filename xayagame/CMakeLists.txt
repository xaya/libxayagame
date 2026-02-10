# Copyright (C) 2018-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# RPC stub generation.
set (XAYAGAME_RPC_STUBS
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/gamerpcclient.h
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/gamerpcserverstub.h
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayarpcclient.h
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayarpcserverstub.h
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayawalletrpcclient.h
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayawalletrpcserverstub.h
)
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/gamerpcclient.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/game.json
    --cpp-client=GameRpcClient
    --cpp-client-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/gamerpcclient.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/game.json
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/gamerpcserverstub.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/game.json
    --cpp-server=GameRpcServerStub
    --cpp-server-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/gamerpcserverstub.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/game.json
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayarpcclient.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya.json
    --cpp-client=XayaRpcClient
    --cpp-client-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayarpcclient.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya.json
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayarpcserverstub.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya.json
    --cpp-server=XayaRpcServerStub
    --cpp-server-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayarpcserverstub.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya.json
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayawalletrpcclient.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya-wallet.json
    --cpp-client=XayaWalletRpcClient
    --cpp-client-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayawalletrpcclient.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya-wallet.json
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayawalletrpcserverstub.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya-wallet.json
    --cpp-server=XayaWalletRpcServerStub
    --cpp-server-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/xayawalletrpcserverstub.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/xaya-wallet.json
)

add_custom_target (xayagame_rpc_stubs DEPENDS ${XAYAGAME_RPC_STUBS})

# Main library.
add_library (xayagame SHARED
  coprocessor.cpp
  defaultmain.cpp
  game.cpp
  gamelogic.cpp
  gamerpcserver.cpp
  heightcache.cpp
  lmdbstorage.cpp
  mainloop.cpp
  pendingmoves.cpp
  pruningqueue.cpp
  rest.cpp
  signatures.cpp
  sqlitegame.cpp
  sqliteintro.cpp
  sqliteproc.cpp
  sqlitestorage.cpp
  storage.cpp
  transactionmanager.cpp
  zmqsubscriber.cpp
)

add_dependencies (xayagame xayagame_rpc_stubs)

target_include_directories (xayagame PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries (xayagame
  PUBLIC
    xayautil
    PkgConfig::JSONCPP
    PkgConfig::JSONRPCCLIENT
    PkgConfig::JSONRPCSERVER
    PkgConfig::GLOG
    PkgConfig::SQLITE3
    PkgConfig::LMDB
  PRIVATE
    PkgConfig::OPENSSL
    PkgConfig::ZLIB
    PkgConfig::CURL
    PkgConfig::MHD
    PkgConfig::GFLAGS
    PkgConfig::ZMQ
    stdc++fs
)

# dumpxayadb binary.
add_executable (dumpxayadb main-dumpdb.cpp)
target_include_directories (dumpxayadb PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (dumpxayadb PRIVATE
  xayagame
  PkgConfig::GLOG
  PkgConfig::GFLAGS
  PkgConfig::SQLITE3
)

# Test utilities library (used by other subdirectories' tests too).
add_library (xayagame_testutils STATIC testutils.cpp)
target_link_libraries (xayagame_testutils PUBLIC
  xayagame
  PkgConfig::JSONCPP
  PkgConfig::JSONRPCCLIENT
  PkgConfig::JSONRPCSERVER
  PkgConfig::GLOG
  GTest::gmock
)

# Unit tests.
add_executable (xayagame_tests
  coprocessor_tests.cpp
  game_tests.cpp
  gamelogic_tests.cpp
  heightcache_tests.cpp
  lmdbstorage_tests.cpp
  mainloop_tests.cpp
  pendingmoves_tests.cpp
  pruningqueue_tests.cpp
  rest_tests.cpp
  signatures_tests.cpp
  sqlitegame_tests.cpp
  sqliteintro_tests.cpp
  sqliteproc_tests.cpp
  sqlitestorage_tests.cpp
  storage_tests.cpp
  transactionmanager_tests.cpp
  zmqsubscriber_tests.cpp
)
target_link_libraries (xayagame_tests PRIVATE
  xayagame_testutils
  xayagame
  PkgConfig::JSONCPP
  PkgConfig::JSONRPCCLIENT
  PkgConfig::JSONRPCSERVER
  PkgConfig::GLOG
  PkgConfig::GFLAGS
  PkgConfig::SQLITE3
  PkgConfig::LMDB
  PkgConfig::ZMQ
  GTest::gmock_main
)
add_test (NAME xayagame_tests COMMAND xayagame_tests)

# Installation.
install (TARGETS xayagame
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install (TARGETS dumpxayadb RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install (FILES
    coprocessor.hpp
    defaultmain.hpp
    game.hpp
    gamelogic.hpp
    gamerpcserver.hpp
    heightcache.hpp
    lmdbstorage.hpp
    mainloop.hpp
    pendingmoves.hpp
    perftimer.hpp
    pruningqueue.hpp
    rest.hpp
    signatures.hpp
    sqlitegame.hpp
    sqliteintro.hpp sqliteintro.tpp
    sqliteproc.hpp
    sqlitestorage.hpp
    storage.hpp
    transactionmanager.hpp
    zmqsubscriber.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xayagame
)
install (FILES ${XAYAGAME_RPC_STUBS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xayagame/rpc-stubs
)

configure_file (libxayagame.pc.in libxayagame.pc @ONLY)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/libxayagame.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
