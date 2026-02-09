# Copyright (C) 2020-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# RPC stub generation.
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/nfrpcserverstub.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/nf.json
    --cpp-server=NFRpcServerStub
    --cpp-server-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/nfrpcserverstub.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/nf.json
)

add_custom_target (nonfungible_rpc_stubs DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/nfrpcserverstub.h
)

# Schema generation.
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema.cpp
  COMMAND ${CMAKE_COMMAND}
    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/schema.cpp
    -DINPUT0=${CMAKE_CURRENT_SOURCE_DIR}/schema_head.cpp
    -DINPUT1=${CMAKE_CURRENT_SOURCE_DIR}/schema.sql
    -DINPUT2=${CMAKE_CURRENT_SOURCE_DIR}/schema_tail.cpp
    -P ${PROJECT_SOURCE_DIR}/cmake/concat_files.cmake
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/schema_head.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/schema.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/schema_tail.cpp
)

# Nonfungible library.
add_library (nonfungible SHARED
  assets.cpp
  logic.cpp
  moveparser.cpp
  moveprocessor.cpp
  pending.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/schema.cpp
  statejson.cpp
)

target_include_directories (nonfungible PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries (nonfungible
  PUBLIC
    xayagame
    xayautil
    PkgConfig::JSONCPP
    PkgConfig::SQLITE3
    PkgConfig::GLOG
)

# nonfungibled binary.
add_executable (nonfungibled
  main.cpp
  rpcserver.cpp
)
add_dependencies (nonfungibled nonfungible_rpc_stubs)
target_include_directories (nonfungibled PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (nonfungibled PRIVATE
  nonfungible
  PkgConfig::GLOG
  PkgConfig::GFLAGS
)

# Unit tests.
add_executable (nonfungible_tests
  assets_tests.cpp
  moveprocessor_tests.cpp
  pending_tests.cpp
  schema_tests.cpp
  statejson_tests.cpp
  testutils.cpp
)
target_link_libraries (nonfungible_tests PRIVATE
  nonfungible
  PkgConfig::JSONCPP
  PkgConfig::SQLITE3
  PkgConfig::GLOG
  GTest::gmock_main
)
add_test (NAME nonfungible_tests COMMAND nonfungible_tests)

# Python integration tests.
set (NONFUNGIBLE_GAMETESTS
  burn.py
  minting.py
  reorg.py
  sqlite_wal.py
  statehash.py
  targetblock.py
  transfers.py
)
foreach (test ${NONFUNGIBLE_GAMETESTS})
  add_test (
    NAME nonfungible_gametest_${test}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gametest/${test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gametest
  )
  set_tests_properties (nonfungible_gametest_${test} PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}:$ENV{PYTHONPATH};BUILD_DIR=${PROJECT_BINARY_DIR}"
  )
endforeach ()

# Installation.
install (TARGETS nonfungibled RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
