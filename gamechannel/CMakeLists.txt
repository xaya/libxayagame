# Copyright (C) 2019-2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

include (sources.cmake)

# Protobuf generation.
include (proto.cmake)
gamechannel_generate_protos (GAMECHANNEL_PROTO_SOURCES GAMECHANNEL_PROTO_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/proto"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
)

# Python stubs (native build only).
gamechannel_generate_proto_py (
  "${CMAKE_CURRENT_SOURCE_DIR}/proto"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

# RPC stub generation.
set (GAMECHANNEL_RPC_STUBS
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/channelgsprpcclient.h
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/channelgsprpcserverstub.h
  ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/rpcbroadcastclient.h
)
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/channelgsprpcclient.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/channel-gsp-rpc.json
    --cpp-client=ChannelGspRpcClient
    --cpp-client-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/channelgsprpcclient.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/channel-gsp-rpc.json
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/channelgsprpcserverstub.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/channel-gsp-rpc.json
    --cpp-server=ChannelGspRpcServerStub
    --cpp-server-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/channelgsprpcserverstub.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/channel-gsp-rpc.json
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/rpcbroadcastclient.h
  COMMAND ${JSONRPCSTUB} ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/rpcbroadcast.json
    --cpp-client=RpcBroadcastClient
    --cpp-client-file=${CMAKE_CURRENT_BINARY_DIR}/rpc-stubs/rpcbroadcastclient.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpc-stubs/rpcbroadcast.json
)

add_custom_target (gamechannel_rpc_stubs DEPENDS ${GAMECHANNEL_RPC_STUBS})

# Schema generation (cat schema_head.cpp + schema.sql + schema_tail.cpp).
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

# channelcore library.
add_library (channelcore STATIC
  ${CHANNELCORE_SOURCES}
  ${GAMECHANNEL_PROTO_SOURCES}
)

set_target_properties (channelcore PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories (channelcore PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries (channelcore
  PUBLIC
    xayautil
    PkgConfig::JSONCPP
    PkgConfig::ETHUTILS
    PkgConfig::GLOG
    PkgConfig::PROTOBUF
)

# gamechannel library.
add_library (gamechannel SHARED
  ${GAMECHANNEL_SOURCES}
  ${CMAKE_CURRENT_BINARY_DIR}/schema.cpp
)

add_dependencies (gamechannel gamechannel_rpc_stubs)

target_include_directories (gamechannel PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries (gamechannel
  PUBLIC
    channelcore
    xayagame
    xayautil
    PkgConfig::JSONCPP
    PkgConfig::JSONRPCCLIENT
    PkgConfig::JSONRPCSERVER
    PkgConfig::GLOG
    PkgConfig::SQLITE3
    PkgConfig::PROTOBUF
)

# Test utilities library.
add_library (gamechannel_testutils STATIC testutils.cpp)
target_link_libraries (gamechannel_testutils PUBLIC
  gamechannel
  channelcore
  PkgConfig::JSONCPP
  PkgConfig::GLOG
  GTest::gmock
)

# Unit tests.
add_executable (gamechannel_tests ${GAMECHANNEL_TEST_SOURCES})
target_link_libraries (gamechannel_tests PRIVATE
  gamechannel_testutils
  gamechannel
  channelcore
  xayagame_testutils
  xayagame
  PkgConfig::JSONCPP
  PkgConfig::JSONRPCCLIENT
  PkgConfig::JSONRPCSERVER
  PkgConfig::GLOG
  PkgConfig::SQLITE3
  PkgConfig::PROTOBUF
  GTest::gmock_main
)
add_test (NAME gamechannel_tests COMMAND gamechannel_tests)

# test_rpcbroadcast binary (used by the Python integration test).
add_executable (test_rpcbroadcast test_rpcbroadcast.cpp)
target_include_directories (test_rpcbroadcast PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (test_rpcbroadcast PRIVATE
  gamechannel
  channelcore
  xayagame
  PkgConfig::JSONCPP
  PkgConfig::JSONRPCCLIENT
  PkgConfig::JSONRPCSERVER
  PkgConfig::GLOG
  PkgConfig::GFLAGS
  PkgConfig::PROTOBUF
)

# Python tests.
add_test (
  NAME gamechannel_signatures_tests.py
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/signatures_tests.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties (gamechannel_signatures_tests.py PROPERTIES
  ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}:$ENV{PYTHONPATH}"
)

add_test (
  NAME gamechannel_test_rpcbroadcast.py
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_rpcbroadcast.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties (gamechannel_test_rpcbroadcast.py PROPERTIES
  ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}:$ENV{PYTHONPATH};BUILD_DIR=${PROJECT_BINARY_DIR}"
)

# Installation.
install (TARGETS channelcore ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install (TARGETS gamechannel
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install (PROGRAMS rpc-channel-server.py DESTINATION ${CMAKE_INSTALL_BINDIR})
install (FILES rpc-stubs/channel-gsp-rpc.json
  DESTINATION ${CMAKE_INSTALL_DATADIR}/gamechannel
)
install (FILES ${CHANNELCORE_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gamechannel
)
install (FILES ${GAMECHANNEL_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gamechannel
)
install (FILES ${GAMECHANNEL_RPC_STUBS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gamechannel/rpc-stubs
)
install (FILES ${GAMECHANNEL_PROTO_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gamechannel/proto
)

configure_file (channelcore.pc.in channelcore.pc @ONLY)
configure_file (gamechannel.pc.in gamechannel.pc @ONLY)
install (FILES
    ${CMAKE_CURRENT_BINARY_DIR}/channelcore.pc
    ${CMAKE_CURRENT_BINARY_DIR}/gamechannel.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
