# XayaWasm.cmake - Reusable CMake module for building Xaya game channel
# games as WebAssembly using Emscripten.
#
# This compiles the complete libchannelcore and xayautil libraries
# (unchanged from native) alongside game-specific logic.  The only
# dependency that is shimmed is glog, which has complex native-only
# dependencies (gflags, libunwind).  All other libraries (protobuf,
# OpenSSL, jsoncpp, eth-utils/secp256k1) are expected to be provided
# as WASM-compiled static libraries.
#
# ============================================================================
# PREREQUISITES
# ============================================================================
#
#   - Emscripten SDK must be active (use emcmake cmake / emmake make)
#   - The following libraries compiled as static WASM libraries:
#       * Protobuf (full, not lite - reflection API needed by protoversion.cpp)
#       * OpenSSL (for SHA-256, base64, cryptographic randomness)
#       * jsoncpp (for JSON serialization)
#       * eth-utils + secp256k1 (for Ethereum ECDSA signatures)
#
# ============================================================================
# USAGE
# ============================================================================
#
# In your game's CMakeLists.txt:
#
#   cmake_minimum_required(VERSION 3.13)
#   project(my-game-wasm LANGUAGES CXX)
#   set(CMAKE_CXX_STANDARD 17)
#   set(CMAKE_CXX_STANDARD_REQUIRED ON)
#
#   # Required: set paths before including this module.
#   set(LIBXAYAGAME_DIR "/path/to/libxayagame")
#   set(PROTOBUF_WASM_DIR "/path/to/protobuf-wasm")
#   set(OPENSSL_WASM_DIR "/path/to/openssl-wasm")
#   set(JSONCPP_WASM_DIR "/path/to/jsoncpp-wasm")
#   set(ETHUTILS_WASM_DIR "/path/to/eth-utils-wasm")
#   set(SECP256K1_WASM_DIR "/path/to/secp256k1-wasm")
#   include(${LIBXAYAGAME_DIR}/wasm/XayaWasm.cmake)
#
#   # Define your game-specific sources and generated proto stubs.
#   set(MY_SOURCES ...)
#   set(MY_PROTO_SOURCES ...)
#
#   add_executable(my-game
#     ${MY_SOURCES}
#     ${XAYA_WASM_GAMECHANNEL_SOURCES}
#     ${XAYA_WASM_XAYAUTIL_SOURCES}
#     ${MY_PROTO_SOURCES}
#     my_bindings.cpp
#   )
#
#   # Add game-specific include paths for generated protos.
#   target_include_directories(my-game PRIVATE
#     "${CMAKE_CURRENT_SOURCE_DIR}/generated/my-game"
#     "${CMAKE_CURRENT_SOURCE_DIR}/generated/gamechannel"
#     "${CMAKE_CURRENT_SOURCE_DIR}/generated"
#   )
#
#   # Apply standard Xaya WASM configuration (include paths, link flags, etc.)
#   xaya_wasm_setup_target(my-game createMyGameModule)
#
# ============================================================================

# Validate that the caller set required variables.
if(NOT DEFINED LIBXAYAGAME_DIR)
  message(FATAL_ERROR "LIBXAYAGAME_DIR must be set before including XayaWasm.cmake")
endif()
if(NOT DEFINED PROTOBUF_WASM_DIR)
  message(FATAL_ERROR "PROTOBUF_WASM_DIR must be set before including XayaWasm.cmake")
endif()
if(NOT DEFINED OPENSSL_WASM_DIR)
  message(FATAL_ERROR "OPENSSL_WASM_DIR must be set before including XayaWasm.cmake")
endif()
if(NOT DEFINED JSONCPP_WASM_DIR)
  message(FATAL_ERROR "JSONCPP_WASM_DIR must be set before including XayaWasm.cmake")
endif()
if(NOT DEFINED ETHUTILS_WASM_DIR)
  message(FATAL_ERROR "ETHUTILS_WASM_DIR must be set before including XayaWasm.cmake")
endif()
if(NOT DEFINED SECP256K1_WASM_DIR)
  message(FATAL_ERROR "SECP256K1_WASM_DIR must be set before including XayaWasm.cmake")
endif()

# Path to the glog shim directory within libxayagame.
set(XAYA_WASM_SHIM_DIR "${LIBXAYAGAME_DIR}/wasm/shims")

# ============================================================================
# Source file lists
# ============================================================================

# Complete libchannelcore (all sources from gamechannel/Makefile.am).
# This is the full library compiled unchanged from native.
set(XAYA_WASM_GAMECHANNEL_SOURCES
  "${LIBXAYAGAME_DIR}/gamechannel/boardrules.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/broadcast.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/channelmanager.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/channelstatejson.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/ethsignatures.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/movesender.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/openchannel.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/protoversion.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/rollingstate.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/signatures.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/stateproof.cpp"
)

# Xaya utility sources (all except compression.cpp which requires zlib
# and is not used by gamechannel).
set(XAYA_WASM_XAYAUTIL_SOURCES
  "${LIBXAYAGAME_DIR}/xayautil/base64.cpp"
  "${LIBXAYAGAME_DIR}/xayautil/cryptorand.cpp"
  "${LIBXAYAGAME_DIR}/xayautil/hash.cpp"
  "${LIBXAYAGAME_DIR}/xayautil/jsonutils.cpp"
  "${LIBXAYAGAME_DIR}/xayautil/random.cpp"
  "${LIBXAYAGAME_DIR}/xayautil/uint256.cpp"
)

# ============================================================================
# xaya_wasm_setup_target(TARGET_NAME EXPORT_NAME)
#
# Configures a CMake target for Xaya WASM channel game compilation.
#
# This sets up:
#   - Include directories (glog shim first for shadowing, then library headers)
#   - Static library linkage (protobuf, OpenSSL, jsoncpp, eth-utils)
#   - Emscripten output settings (WASM, modularized JS, embind)
#   - Compiler warning flags and optimization level
#
# Arguments:
#   TARGET_NAME  - The CMake target name (e.g., "ships-channel")
#   EXPORT_NAME  - The JS module export name (e.g., "createShipsModule")
# ============================================================================
function(xaya_wasm_setup_target TARGET_NAME EXPORT_NAME)

  # Include paths: ORDER MATTERS.
  # The glog shim must come first so it shadows the native glog/logging.h
  # header.  All other dependencies use their real headers from the
  # WASM-compiled library installations.
  target_include_directories(${TARGET_NAME} BEFORE PRIVATE
    # 1. Glog shim FIRST - shadows glog/logging.h with a minimal stub
    #    (LOG/CHECK/VLOG macros that discard output, abort on fatal).
    "${XAYA_WASM_SHIM_DIR}"

    # 2. libxayagame root - for angle-bracket includes like
    #    <gamechannel/...> and <xayautil/...>.
    "${LIBXAYAGAME_DIR}"

    # 3. gamechannel subdirectory - for relative includes within
    #    gamechannel source files (e.g., #include "boardrules.hpp").
    "${LIBXAYAGAME_DIR}/gamechannel"

    # 4. WASM-compiled dependency headers.
    "${PROTOBUF_WASM_DIR}/include"
    "${OPENSSL_WASM_DIR}/include"
    "${JSONCPP_WASM_DIR}/include"
    "${ETHUTILS_WASM_DIR}/include"
    "${SECP256K1_WASM_DIR}/include"
  )

  # Link WASM-compiled static libraries.
  # Order matters: eth-utils depends on secp256k1, so secp256k1 comes after.
  target_link_libraries(${TARGET_NAME}
    "${PROTOBUF_WASM_DIR}/lib/libprotobuf.a"
    "${OPENSSL_WASM_DIR}/lib/libssl.a"
    "${OPENSSL_WASM_DIR}/lib/libcrypto.a"
    "${JSONCPP_WASM_DIR}/lib/libjsoncpp.a"
    "${ETHUTILS_WASM_DIR}/lib/libeth-utils.a"
    "${SECP256K1_WASM_DIR}/lib/libsecp256k1.a"
  )

  # Emscripten output settings.
  set_target_properties(${TARGET_NAME} PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "\
      --bind \
      -s WASM=1 \
      -s ALLOW_MEMORY_GROWTH=1 \
      -s MODULARIZE=1 \
      -s EXPORT_NAME=${EXPORT_NAME} \
      -s ENVIRONMENT=web,worker,node \
      -s NO_EXIT_RUNTIME=1 \
      -s DISABLE_EXCEPTION_CATCHING=0 \
    "
  )

  # Compiler flags: optimize for size/speed, enable warnings.
  target_compile_options(${TARGET_NAME} PRIVATE
    -O2
    -Wall
    -Wno-unused-variable
    -Wno-unused-but-set-variable
  )

endfunction()
