# XayaWasm.cmake - Reusable CMake module for building Xaya game channel
# games as WebAssembly using Emscripten.
#
# This module provides shared source file lists, include path setup,
# and Emscripten configuration so that individual game projects do not
# need to duplicate WASM build infrastructure.
#
# ============================================================================
# PREREQUISITES
# ============================================================================
#
#   - Emscripten SDK must be active (use emcmake cmake / emmake make)
#   - Protobuf must be compiled for WASM (static library)
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
#     ${XAYA_WASM_SHIM_SOURCES}
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

# Path to the WASM shims directory within libxayagame.
set(XAYA_WASM_SHIM_DIR "${LIBXAYAGAME_DIR}/wasm/shims")

# ============================================================================
# Source file lists
# ============================================================================

# Game channel framework sources (from libxayagame, used unchanged).
# These provide the core channel logic: board rules, state proofs,
# signatures, and move sending.
set(XAYA_WASM_GAMECHANNEL_SOURCES
  "${LIBXAYAGAME_DIR}/gamechannel/boardrules.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/openchannel.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/signatures.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/stateproof.cpp"
  "${LIBXAYAGAME_DIR}/gamechannel/movesender.cpp"
)

# Xaya utility sources (from libxayagame, used unchanged).
# uint256 for hash representation, random for deterministic PRNG.
set(XAYA_WASM_XAYAUTIL_SOURCES
  "${LIBXAYAGAME_DIR}/xayautil/uint256.cpp"
  "${LIBXAYAGAME_DIR}/xayautil/random.cpp"
)

# WASM shim sources replacing native-only dependencies.
# These provide standalone implementations of SHA-256, base64,
# cryptographic randomness, and simplified proto version checks
# without requiring OpenSSL, glog, or jsoncpp.
set(XAYA_WASM_SHIM_SOURCES
  "${XAYA_WASM_SHIM_DIR}/sha256.cpp"
  "${XAYA_WASM_SHIM_DIR}/hash_shim.cpp"
  "${XAYA_WASM_SHIM_DIR}/base64_shim.cpp"
  "${XAYA_WASM_SHIM_DIR}/cryptorand_shim.cpp"
  "${XAYA_WASM_SHIM_DIR}/protoversion_shim.cpp"
)

# ============================================================================
# xaya_wasm_setup_target(TARGET_NAME EXPORT_NAME)
#
# Configures a CMake target for Xaya WASM channel game compilation.
#
# This sets up:
#   - Include directories with correct shadowing order (shims first)
#   - Protobuf static library linkage
#   - Emscripten output settings (WASM, modularized JS, embind)
#   - Compiler warning flags and optimization level
#
# Arguments:
#   TARGET_NAME  - The CMake target name (e.g., "ships-channel")
#   EXPORT_NAME  - The JS module export name (e.g., "createShipsModule")
# ============================================================================
function(xaya_wasm_setup_target TARGET_NAME EXPORT_NAME)

  # Include paths: ORDER MATTERS.
  # Shims must come first so they shadow the native glog, jsoncpp,
  # and gamechannel/protoversion.hpp headers.
  target_include_directories(${TARGET_NAME} BEFORE PRIVATE
    # 1. Shims FIRST - shadows glog/logging.h, json/json.h, json/writer.h,
    #    and gamechannel/protoversion.hpp with WASM-compatible versions.
    "${XAYA_WASM_SHIM_DIR}"

    # 2. libxayagame root - for angle-bracket includes like
    #    <gamechannel/...> and <xayautil/...>.
    "${LIBXAYAGAME_DIR}"

    # 3. gamechannel subdirectory - for relative includes within
    #    gamechannel source files (e.g., #include "boardrules.hpp").
    "${LIBXAYAGAME_DIR}/gamechannel"

    # 4. Protobuf headers from the WASM protobuf installation.
    "${PROTOBUF_WASM_DIR}/include"
  )

  # Link the WASM-compiled protobuf static library.
  target_link_libraries(${TARGET_NAME}
    "${PROTOBUF_WASM_DIR}/lib/libprotobuf.a"
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
