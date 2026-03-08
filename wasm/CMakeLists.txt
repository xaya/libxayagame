# Copyright (C) 2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Standalone CMake project for building the Xaya WASM libraries.
# Must be configured with the Emscripten toolchain:
#
#   emcmake cmake -B build wasm/ \
#     -DCMAKE_INSTALL_PREFIX=/opt/wasm \
#     -DWASM_DIR=/opt/wasm
#   cmake --build build
#   cmake --install build

cmake_minimum_required (VERSION 3.14)
project (xayagame-wasm LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

if (NOT EMSCRIPTEN)
  message (FATAL_ERROR
    "This CMakeLists.txt must be configured with the Emscripten toolchain. "
    "Use: emcmake cmake ...")
endif ()

if (NOT DEFINED WASM_DIR)
  message (FATAL_ERROR "WASM_DIR must be set to the WASM dependencies prefix "
    "(e.g. -DWASM_DIR=/opt/wasm)")
endif ()

add_compile_options (-Wall -Wno-unused-variable -Wno-unused-but-set-variable)
add_compile_definitions (GLOG_NO_ABBREVIATED_SEVERITIES)

# zlib is provided by Emscripten's built-in port.
add_compile_options (-sUSE_ZLIB=1)

set (LIBXAYAGAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

# ============================================================================
# xayautil
# ============================================================================

include (${LIBXAYAGAME_DIR}/xayautil/sources.cmake)

set (_xayautil_sources)
foreach (src ${XAYAUTIL_SOURCES})
  list (APPEND _xayautil_sources "${LIBXAYAGAME_DIR}/xayautil/${src}")
endforeach ()

add_library (xayautil STATIC ${_xayautil_sources})
target_include_directories (xayautil PUBLIC
  $<BUILD_INTERFACE:${LIBXAYAGAME_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_include_directories (xayautil PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/shims"
  "${WASM_DIR}/include"
)
target_compile_options (xayautil PRIVATE -O2)

# ============================================================================
# channelcore
# ============================================================================

include (${LIBXAYAGAME_DIR}/gamechannel/sources.cmake)
include (${LIBXAYAGAME_DIR}/gamechannel/proto.cmake)

gamechannel_generate_protos (GAMECHANNEL_PROTO_SOURCES GAMECHANNEL_PROTO_HEADERS
  "${LIBXAYAGAME_DIR}/gamechannel/proto"
  "${LIBXAYAGAME_DIR}"
  "${CMAKE_BINARY_DIR}"
)

set (_channelcore_sources)
foreach (src ${CHANNELCORE_SOURCES})
  list (APPEND _channelcore_sources "${LIBXAYAGAME_DIR}/gamechannel/${src}")
endforeach ()

add_library (channelcore STATIC
  ${_channelcore_sources}
  ${GAMECHANNEL_PROTO_SOURCES}
)
target_include_directories (channelcore PUBLIC
  $<BUILD_INTERFACE:${LIBXAYAGAME_DIR}>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_include_directories (channelcore PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/shims"
  "${LIBXAYAGAME_DIR}/gamechannel"
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_BINARY_DIR}/gamechannel"
  "${WASM_DIR}/include"
)
target_compile_options (channelcore PRIVATE -O2)
target_link_libraries (channelcore PUBLIC xayautil)

# ============================================================================
# Installation
# ============================================================================

include (GNUInstallDirs)

install (TARGETS xayautil channelcore
  EXPORT XayaGameWasmTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# xayautil headers.
set (_xayautil_headers_abs)
foreach (hdr ${XAYAUTIL_HEADERS})
  list (APPEND _xayautil_headers_abs "${LIBXAYAGAME_DIR}/xayautil/${hdr}")
endforeach ()
install (FILES ${_xayautil_headers_abs}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xayautil
)

# channelcore headers.
set (_channelcore_headers_abs)
foreach (hdr ${CHANNELCORE_HEADERS})
  list (APPEND _channelcore_headers_abs "${LIBXAYAGAME_DIR}/gamechannel/${hdr}")
endforeach ()
install (FILES ${_channelcore_headers_abs}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gamechannel
)

# Generated protobuf headers.
install (FILES ${GAMECHANNEL_PROTO_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gamechannel/proto
)

# Glog shim headers (needed by consumers at compile time).
# Installed into a private namespaced subdirectory to avoid colliding with
# real glog headers or shims from other packages in the same prefix.
install (DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shims/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xayagame-wasm/shims
)

# CMake package config.
install (EXPORT XayaGameWasmTargets
  NAMESPACE XayaGameWasm::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/XayaGameWasm
)
configure_file (XayaGameWasmConfig.cmake.in XayaGameWasmConfig.cmake @ONLY)
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/XayaGameWasmConfig.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/XayaGameWasm
)
