# WASM Build Support for libxayagame

This directory provides infrastructure for compiling Xaya game channel
games to WebAssembly (WASM) using Emscripten.  This allows browser clients
to run the exact same C++ game logic as the native Game State Processor (GSP),
ensuring consensus correctness between all participants.

## Architecture

The WASM build compiles the **complete libchannelcore** library and
xaya utilities (`xayautil/`) alongside game-specific logic into a single
`.wasm` binary.  The C++ source files are compiled **unchanged** from
native — the only shimmed dependency is glog.

### Dependencies

All libraries except glog are cross-compiled to WASM static libraries
and linked normally:

| Library | Purpose |
|---------|---------|
| Protobuf (full, not lite) | Protocol buffer serialization and reflection API |
| OpenSSL | SHA-256 hashing, base64 encoding, cryptographic randomness |
| jsoncpp | JSON serialization for channel state |
| eth-utils + secp256k1 | Ethereum ECDSA signatures (signing and verification) |

Full protobuf (not lite) is required because `protoversion.cpp` uses
the reflection API (`GetReflection`, `GetUnknownFields`).

### Glog Shim (`wasm/shims/`)

The only shim is a minimal replacement for glog (`glog_stub.hpp`).
Glog has complex native-only dependencies (gflags, libunwind, etc.)
that make cross-compilation impractical.  The shim provides:

- `LOG(severity)` — discards output, aborts on `FATAL`
- `LOG_IF(severity, cond)` — conditional logging
- `LOG_FIRST_N(severity, n)` — throttled logging (no-op in stub)
- `VLOG(level)` — verbose logging (discarded)
- `CHECK(cond)`, `CHECK_EQ`, `CHECK_NE`, etc. — abort on failure

The shim intentionally avoids defining severity level constants as
macros (e.g., `#define FATAL 3`) because this conflicts with protobuf's
internal logging which uses token pasting (`LOGLEVEL_##LEVEL`).

The shim is placed first in the include path so `#include <glog/logging.h>`
resolves to the stub without any `#ifdef` guards in the source code.

### What's Compiled

**libchannelcore (complete):**
- `boardrules.cpp` — Abstract game board interface
- `broadcast.cpp` — Off-chain move broadcast handling
- `channelmanager.cpp` — Main channel orchestrator
- `channelstatejson.cpp` — Channel state JSON serialization
- `ethsignatures.cpp` — Ethereum ECDSA signature verification/signing
- `movesender.cpp` — Move sending interface
- `openchannel.cpp` — Channel opening/joining logic
- `protoversion.cpp` — Protocol version checking (full reflection)
- `rollingstate.cpp` — Channel state tracking across reinits
- `signatures.cpp` — Abstract signature interfaces
- `stateproof.cpp` — Cryptographic state proofs

**xayautil:**
- `base64.cpp` — Base64 encoding/decoding (via OpenSSL)
- `cryptorand.cpp` — Cryptographic random number generation (via OpenSSL)
- `hash.cpp` — SHA-256 hashing (via OpenSSL)
- `jsonutils.cpp` — JSON utility functions (via jsoncpp)
- `random.cpp` — Deterministic PRNG
- `uint256.cpp` — 256-bit hash representation

### CMake Module (`XayaWasm.cmake`)

A reusable CMake `include()` module that provides:
- Source file lists for gamechannel and xayautil
- `xaya_wasm_setup_target()` function for Emscripten configuration
- Correct include path ordering (glog shim first for shadowing)
- Static library linkage for all WASM dependencies

## Prerequisites

1. **Emscripten SDK** — Install and activate (`source emsdk_env.sh`)
2. **Protobuf for WASM** — Full protobuf compiled as a static WASM library
3. **OpenSSL for WASM** — OpenSSL compiled as a static WASM library
4. **jsoncpp for WASM** — jsoncpp compiled as a static WASM library
5. **eth-utils for WASM** — eth-utils (with secp256k1) compiled as a static WASM library
6. **libxayagame source** — This repository

## Usage

### 1. Generate protobuf stubs

Each game generates its own protobuf C++ stubs for both game-specific
and gamechannel protos:

```bash
# Game-specific protos
protoc --cpp_out="generated/my-game" -I"my-game/proto" boardstate.proto boardmove.proto

# Gamechannel protos (ALL of them)
protoc --cpp_out="generated/gamechannel" -I"$LIBXAYAGAME_DIR" \
  gamechannel/proto/metadata.proto \
  gamechannel/proto/signatures.proto \
  gamechannel/proto/stateproof.proto \
  gamechannel/proto/broadcast.proto
```

### 2. Write CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.13)
project(my-game-wasm LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set paths to libxayagame and WASM-compiled dependencies.
set(LIBXAYAGAME_DIR "/path/to/libxayagame")
set(PROTOBUF_WASM_DIR "/path/to/protobuf-wasm")
set(OPENSSL_WASM_DIR "/path/to/openssl-wasm")
set(JSONCPP_WASM_DIR "/path/to/jsoncpp-wasm")
set(ETHUTILS_WASM_DIR "/path/to/eth-utils-wasm")
set(SECP256K1_WASM_DIR "/path/to/secp256k1-wasm")
include(${LIBXAYAGAME_DIR}/wasm/XayaWasm.cmake)

# Game-specific sources (the complete game core library).
set(GAME_SOURCES
  my_board.cpp
  my_channel.cpp
)

# Generated protobuf stubs (game + ALL gamechannel protos).
set(PROTO_SOURCES
  generated/my-game/proto/boardstate.pb.cc
  generated/gamechannel/proto/metadata.pb.cc
  generated/gamechannel/proto/signatures.pb.cc
  generated/gamechannel/proto/stateproof.pb.cc
  generated/gamechannel/proto/broadcast.pb.cc
)

# Build the WASM module.
add_executable(my-game
  ${GAME_SOURCES}
  ${XAYA_WASM_GAMECHANNEL_SOURCES}
  ${XAYA_WASM_XAYAUTIL_SOURCES}
  ${PROTO_SOURCES}
  bindings/wasm_bindings.cpp
)

# Game-specific include paths for generated protos.
target_include_directories(my-game PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/my-game"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/gamechannel"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated"
)

# Apply Xaya WASM configuration (glog shim, dependency linking, etc.)
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

Each game writes its own Emscripten embind bindings to expose
game-specific C++ classes and functions to JavaScript.  Games should
use the `ChannelManager` and other libchannelcore classes directly
via embind rather than reimplementing their logic in JavaScript.

## Provided Variables (after include)

| Variable | Contents |
|----------|----------|
| `XAYA_WASM_SHIM_DIR` | Path to `wasm/shims/` (glog stub) |
| `XAYA_WASM_GAMECHANNEL_SOURCES` | Complete libchannelcore |
| `XAYA_WASM_XAYAUTIL_SOURCES` | xayautil (base64, hash, random, uint256, etc.) |
