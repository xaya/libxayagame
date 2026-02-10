# Copyright (C) 2019-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Protobuf generation.
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)

set (GAMECHANNEL_PROTOS
  broadcast
  metadata
  signatures
  stateproof
  testprotos
)

set (GAMECHANNEL_PROTO_SOURCES)
set (GAMECHANNEL_PROTO_HEADERS)
set (GAMECHANNEL_PROTO_PY)
foreach (proto ${GAMECHANNEL_PROTOS})
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
  list (APPEND GAMECHANNEL_PROTO_SOURCES ${cc})
  list (APPEND GAMECHANNEL_PROTO_HEADERS ${h})
  list (APPEND GAMECHANNEL_PROTO_PY ${py})
endforeach ()

add_custom_target (gamechannel_proto_py ALL DEPENDS ${GAMECHANNEL_PROTO_PY})

# Copy Python source files into the build tree so that the gamechannel
# Python package is complete (source .py + generated _pb2.py together).
set (GAMECHANNEL_PY_SOURCES
  __init__.py
  channeltest.py
  rpcbroadcast.py
  signatures.py
)
foreach (pyfile ${GAMECHANNEL_PY_SOURCES})
  configure_file (${pyfile} ${PROJECT_BINARY_DIR}/gamechannel/${pyfile} COPYONLY)
endforeach ()
configure_file (proto/__init__.py ${PROJECT_BINARY_DIR}/gamechannel/proto/__init__.py COPYONLY)

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
  channelgame.cpp
  chaintochannel.cpp
  daemon.cpp
  database.cpp
  gamestatejson.cpp
  gsprpc.cpp
  recvbroadcast.cpp
  rpcbroadcast.cpp
  rpcwallet.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/schema.cpp
  syncmanager.cpp
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
add_executable (gamechannel_tests
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
install (FILES
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
