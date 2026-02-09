# Copyright (C) 2018-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Protobuf generation.
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)

add_custom_command (
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/proto/mover.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/proto/mover.pb.h
  COMMAND protobuf::protoc
    -I${CMAKE_CURRENT_SOURCE_DIR}
    --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/mover.proto
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/proto/mover.proto
)

# Mover library.
add_library (mover SHARED
  logic.cpp
  moves.cpp
  pending.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/proto/mover.pb.cc
)

target_include_directories (mover PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries (mover
  PUBLIC
    xayagame
    xayautil
    PkgConfig::JSONCPP
    PkgConfig::GLOG
    PkgConfig::PROTOBUF
)

# moverd binary.
add_executable (moverd main.cpp)
target_include_directories (moverd PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (moverd PRIVATE
  mover
  PkgConfig::GFLAGS
  PkgConfig::PROTOBUF
)

# Unit tests.
add_executable (mover_tests
  logic_tests.cpp
  moves_tests.cpp
  pending_tests.cpp
)
target_link_libraries (mover_tests PRIVATE
  mover
  PkgConfig::JSONCPP
  PkgConfig::GLOG
  PkgConfig::PROTOBUF
  GTest::gmock_main
)
add_test (NAME mover_tests COMMAND mover_tests)

# Python integration tests.
set (MOVER_GAMETESTS
  basic.py
  bind_locally.py
  catching_up.py
  dupjsonkeys.py
  getnullstate.py
  pending-disabled.py
  pending-onesocket.py
  pending-twosockets.py
  persistence-lmdb.py
  persistence-sqlite.py
  pruning.py
  reorg.py
  stopped_xayad.py
  waitforchange.py
  xayarpcwait.py
)
foreach (test ${MOVER_GAMETESTS})
  add_test (
    NAME mover_gametest_${test}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gametest/${test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gametest
  )
  set_tests_properties (mover_gametest_${test} PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}:$ENV{PYTHONPATH};BUILD_DIR=${PROJECT_BINARY_DIR}"
  )
endforeach ()

# Installation.
install (TARGETS moverd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
