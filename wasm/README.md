# WASM Build Support for libxayagame

This directory provides the infrastructure for compiling Xaya game channel
games to WebAssembly (WASM) using Emscripten. This allows browser clients
to run the exact same C++ game logic as the native Game State Processor (GSP),
ensuring consensus correctness between all participants.

## Architecture

The WASM build compiles the game channel framework (`gamechannel/`) and
xaya utilities (`xayautil/`) alongside game-specific logic into a single
`.wasm` binary. Since the WASM environment lacks OpenSSL, glog, and jsoncpp,
lightweight **shims** replace these dependencies.

### Shims (`wasm/shims/`)

| Shim | Replaces | Purpose |
|------|----------|---------|
| `glog_stub.hpp`, `glog/logging.h` | `glog` | Minimal LOG/CHECK macros (abort on fatal, discard output) |
| `json_stub.hpp`, `json/json.h`, `json/writer.h` | `jsoncpp` | No-op Json::Value (display handled by JS frontend) |
| `sha256.h`, `sha256.cpp` | OpenSSL EVP SHA-256 | Standalone SHA-256 (public domain) |
| `hash_shim.cpp` | `xayautil/hash.cpp` | `xaya::SHA256` backed by standalone sha256 |
| `base64_shim.cpp` | `xayautil/base64.cpp` | `xaya::EncodeBase64/DecodeBase64` without OpenSSL |
| `cryptorand_shim.cpp` | `xayautil/cryptorand.cpp` | `xaya::CryptoRand` via Emscripten's `getentropy()` |
| `protoversion_shim.cpp`, `gamechannel/protoversion.hpp` | `gamechannel/protoversion.cpp` | Simplified always-pass checks (GSP validates server-side) |

The shims are placed first in the include path so they shadow the native
headers. This means the same C++ source files compile against either the
native dependencies (autotools build) or the WASM shims (CMake/Emscripten
build) without any `#ifdef` guards.

### CMake Module (`XayaWasm.cmake`)

A reusable CMake `include()` module that provides:
- Source file lists for gamechannel, xayautil, and shim sources
- `xaya_wasm_setup_target()` function for Emscripten configuration
- Correct include path ordering (shims first for shadowing)

## Prerequisites

1. **Emscripten SDK** - Install and activate (`source emsdk_env.sh`)
2. **Protobuf for WASM** - Protocol Buffers compiled as a static library
   for the Emscripten target (installed at a path like `/tmp/protobuf-wasm`)
3. **libxayagame source** - This repository

## Usage

### 1. Generate protobuf stubs

Each game generates its own protobuf C++ stubs for both game-specific
and gamechannel protos:

```bash
# Game-specific protos
protoc --cpp_out="generated/my-game" -I"my-game/proto" boardstate.proto boardmove.proto

# Gamechannel protos (metadata, signatures, stateproof)
protoc --cpp_out="generated/gamechannel" -I"$LIBXAYAGAME_DIR" \
  gamechannel/proto/metadata.proto \
  gamechannel/proto/signatures.proto \
  gamechannel/proto/stateproof.proto
```

### 2. Write CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.13)
project(my-game-wasm LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set paths and include the Xaya WASM module.
set(LIBXAYAGAME_DIR "/path/to/libxayagame")
set(PROTOBUF_WASM_DIR "/path/to/protobuf-wasm")
include(${LIBXAYAGAME_DIR}/wasm/XayaWasm.cmake)

# Game-specific sources.
set(GAME_SOURCES
  my_board.cpp
  my_channel.cpp
)

# Generated protobuf stubs.
set(PROTO_SOURCES
  generated/my-game/proto/boardstate.pb.cc
  generated/gamechannel/proto/metadata.pb.cc
  generated/gamechannel/proto/signatures.pb.cc
  generated/gamechannel/proto/stateproof.pb.cc
)

# Build the WASM module.
add_executable(my-game
  ${GAME_SOURCES}
  ${XAYA_WASM_GAMECHANNEL_SOURCES}
  ${XAYA_WASM_XAYAUTIL_SOURCES}
  ${XAYA_WASM_SHIM_SOURCES}
  ${PROTO_SOURCES}
  bindings/wasm_bindings.cpp
)

# Game-specific include paths for generated protos.
target_include_directories(my-game PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/my-game"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/gamechannel"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated"
)

# Apply Xaya WASM configuration (shim includes, Emscripten flags, etc.)
xaya_wasm_setup_target(my-game createMyGameModule)
```

### 3. Build

```bash
cd my-game/wasm/build
emcmake cmake ..
emmake make -j$(nproc)
```

Output: `my-game.js` + `my-game.wasm`

### 4. Embind bindings

Each game writes its own Emscripten embind bindings
(`wasm_bindings.cpp`) to expose game-specific C++ classes and
functions to JavaScript. See the xayaships and ironman projects
for examples.

## Provided Variables (after include)

| Variable | Contents |
|----------|----------|
| `XAYA_WASM_SHIM_DIR` | Path to `wasm/shims/` |
| `XAYA_WASM_GAMECHANNEL_SOURCES` | boardrules, openchannel, signatures, stateproof, movesender |
| `XAYA_WASM_XAYAUTIL_SOURCES` | uint256, random |
| `XAYA_WASM_SHIM_SOURCES` | sha256, hash_shim, base64_shim, cryptorand_shim, protoversion_shim |

## Keeping Shims in Sync

The shim `.cpp` files implement the same public API as the originals they
replace. If the API of `xayautil/hash.hpp`, `xayautil/base64.hpp`,
`xayautil/cryptorand.hpp`, or `gamechannel/protoversion.hpp` changes,
the corresponding shim must be updated to match.
