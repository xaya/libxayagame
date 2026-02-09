# Copyright (C) 2019-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Protobuf generation.
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)

set (SHIPS_PROTOS boardmove boardstate)
set (SHIPS_PROTO_SOURCES)
set (SHIPS_PROTO_HEADERS)
set (SHIPS_PROTO_PY)
foreach (proto ${SHIPS_PROTOS})
  set (src ${CMAKE_CURRENT_SOURCE_DIR}/proto/${proto}.proto)
  set (cc ${CMAKE_CURRENT_BINARY_DIR}/proto/${proto}.pb.cc)
  set (h ${CMAKE_CURRENT_BINARY_DIR}/proto/${proto}.pb.h)
  set (py ${CMAKE_CURRENT_BINARY_DIR}/proto/${proto}_pb2.py)
  add_custom_command (
    OUTPUT ${cc} ${h}
    COMMAND protobuf::protoc
      -I${PROJECT_SOURCE_DIR}
      --cpp_out=${PROJECT_BINARY_DIR}
      ${src}
    DEPENDS ${src}
  )
  add_custom_command (
    OUTPUT ${py}
    COMMAND protobuf::protoc
      -I${PROJECT_SOURCE_DIR}
      --python_out=${PROJECT_BINARY_DIR}
      ${src}
    DEPENDS ${src}
  )
  list (APPEND SHIPS_PROTO_SOURCES ${cc})
  list (APPEND SHIPS_PROTO_HEADERS ${h})
  list (APPEND SHIPS_PROTO_PY ${py})
endforeach ()

add_custom_target (ships_proto_py ALL DEPENDS ${SHIPS_PROTO_PY})

# Copy proto __init__.py into the build tree so the ships.proto Python
# package is complete alongside the generated _pb2.py files.
configure_file (proto/__init__.py ${CMAKE_CURRENT_BINARY_DIR}/proto/__init__.py COPYONLY)

# RPC stub generation.
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/shipschannelrpcserverstub.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/channel.json
    --cpp-server=ShipsChannelRpcServerStub
    --cpp-server-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/shipschannelrpcserverstub.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/channel.json
)

add_custom_target (ships_rpc_stubs DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/shipschannelrpcserverstub.h
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

# shipscore library (core game logic, no GSP dependencies).
add_library (shipscore STATIC
  board.cpp
  channel.cpp
  coord.cpp
  grid.cpp
  ${SHIPS_PROTO_SOURCES}
)

set_target_properties (shipscore PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories (shipscore PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries (shipscore
  PUBLIC
    channelcore
    xayautil
    PkgConfig::JSONCPP
    PkgConfig::GLOG
    PkgConfig::PROTOBUF
)

# ships library (GSP logic).
add_library (ships SHARED
  gamestatejson.cpp
  logic.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/schema.cpp
)

target_include_directories (ships PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries (ships
  PUBLIC
    shipscore
    gamechannel
    channelcore
    xayagame
    xayautil
    PkgConfig::JSONCPP
    PkgConfig::SQLITE3
    PkgConfig::GLOG
    PkgConfig::PROTOBUF
)

# shipsd binary.
add_executable (shipsd main-gsp.cpp)
target_include_directories (shipsd PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (shipsd PRIVATE
  ships
  PkgConfig::JSONCPP
  PkgConfig::GLOG
  PkgConfig::GFLAGS
  PkgConfig::PROTOBUF
)

# ships-channel binary.
add_executable (ships-channel
  main-channel.cpp
  channelrpc.cpp
)
add_dependencies (ships-channel ships_rpc_stubs)
target_include_directories (ships-channel PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (ships-channel PRIVATE
  ships
  PkgConfig::JSONCPP
  PkgConfig::JSONRPCSERVER
  PkgConfig::GLOG
  PkgConfig::GFLAGS
  PkgConfig::PROTOBUF
)

# Unit tests.
add_executable (ships_tests
  board_tests.cpp
  channel_tests.cpp
  coord_tests.cpp
  gamestatejson_tests.cpp
  grid_tests.cpp
  logic_tests.cpp
  schema_tests.cpp
  testutils.cpp
)
target_link_libraries (ships_tests PRIVATE
  ships
  shipscore
  gamechannel_testutils
  gamechannel
  xayagame_testutils
  xayagame
  channelcore
  PkgConfig::JSONCPP
  PkgConfig::GLOG
  PkgConfig::PROTOBUF
  GTest::gmock_main
)
add_test (NAME ships_tests COMMAND ships_tests)

# Python integration tests — gametest.
set (SHIPS_GAMETESTS
  channel_management.py
  disputes.py
  force_close.py
  getchannel.py
  pending.py
  reorg.py
)
foreach (test ${SHIPS_GAMETESTS})
  add_test (
    NAME ships_gametest_${test}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gametest/${test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gametest
  )
  set_tests_properties (ships_gametest_${test} PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_BINARY_DIR}/ships:${PROJECT_SOURCE_DIR}:${PROJECT_SOURCE_DIR}/ships:$ENV{PYTHONPATH};BUILD_DIR=${PROJECT_BINARY_DIR}"
  )
endforeach ()

# Python integration tests — channeltest.
set (SHIPS_CHANNELTESTS
  disputes.py
  force_close.py
  full_game.py
  pending.py
  reorg.py
  tx_fail.py
  validateposition.py
  waitforchange.py
)
foreach (test ${SHIPS_CHANNELTESTS})
  add_test (
    NAME ships_channeltest_${test}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/channeltest/${test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/channeltest
  )
  set_tests_properties (ships_channeltest_${test} PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_BINARY_DIR}/ships:${PROJECT_SOURCE_DIR}:${PROJECT_SOURCE_DIR}/ships:${PROJECT_SOURCE_DIR}/ships/gametest:$ENV{PYTHONPATH};BUILD_DIR=${PROJECT_BINARY_DIR}"
  )
endforeach ()

# Installation.
install (TARGETS shipsd ships-channel RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
