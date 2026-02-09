# Copyright (C) 2018-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

cmake_minimum_required (VERSION 3.14)
project (libxayagame VERSION 1.0.3 LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set (CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif ()

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

add_compile_options (-Wall -Werror -pedantic -Wno-array-bounds -Wno-deprecated)

# Windows defines ERROR, which requires us to tell glog to not define
# it as abbreviated log severity (LOG(ERROR) still works, though).
add_compile_definitions (GLOG_NO_ABBREVIATED_SEVERITIES)

configure_file (cmake/config.h.in config.h)

include (GNUInstallDirs)
find_package (PkgConfig REQUIRED)
find_package (Protobuf REQUIRED)
find_program (JSONRPCSTUB jsonrpcstub REQUIRED)
find_package (Python3 REQUIRED COMPONENTS Interpreter)

# Dependencies.
pkg_check_modules (JSONCPP REQUIRED IMPORTED_TARGET jsoncpp>=1.7.5)
pkg_check_modules (GLOG REQUIRED IMPORTED_TARGET libglog)
pkg_check_modules (OPENSSL REQUIRED IMPORTED_TARGET openssl)
pkg_check_modules (PROTOBUF REQUIRED IMPORTED_TARGET protobuf)
pkg_check_modules (ETHUTILS REQUIRED IMPORTED_TARGET ethutils)
pkg_check_modules (ZLIB REQUIRED IMPORTED_TARGET zlib>=1.2.11)
pkg_check_modules (ZMQ REQUIRED IMPORTED_TARGET libzmq>=4.3.1)
pkg_check_modules (SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules (LMDB REQUIRED IMPORTED_TARGET lmdb)
pkg_check_modules (CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules (MHD REQUIRED IMPORTED_TARGET libmicrohttpd)
pkg_check_modules (GFLAGS REQUIRED IMPORTED_TARGET gflags)
pkg_check_modules (JSONRPCCLIENT REQUIRED IMPORTED_TARGET libjsonrpccpp-client)
pkg_check_modules (JSONRPCSERVER REQUIRED IMPORTED_TARGET libjsonrpccpp-server)

# SQLite needs the session extension defines.
set_property (TARGET PkgConfig::SQLITE3 APPEND PROPERTY
  INTERFACE_COMPILE_DEFINITIONS
    SQLITE_ENABLE_SESSION SQLITE_ENABLE_PREUPDATE_HOOK
)

# Testing.
find_package (GTest REQUIRED)
enable_testing ()

add_subdirectory (xayautil)
add_subdirectory (xayagame)
add_subdirectory (gamechannel)
add_subdirectory (mover)
add_subdirectory (nonfungible)
add_subdirectory (ships)
add_subdirectory (websocket)
