# WASM Build Support for libxayagame

This directory provides infrastructure for compiling the Xaya channel
game libraries (`xayautil` and `channelcore`) to WebAssembly (WASM) using
Emscripten, and for installing them as a conventional CMake package.  Game
projects then simply do `find_package (XayaGameWasm REQUIRED)` to link
against the pre-built libraries — the same pattern as the native build.

## Architecture

### Two-stage workflow

1. **Build and install the WASM libraries** (once, from this repository).
   `wasm/CMakeLists.txt` is a standalone CMake project that compiles
   `xayautil` and `channelcore` with Emscripten and installs them, along
   with all public headers and a CMake package config file, under a chosen
   prefix (e.g. `/opt/wasm`).

2. **Build the game** (in the game's own repository).
   The game project calls `find_package (XayaGameWasm REQUIRED)` with
   `CMAKE_PREFIX_PATH` pointing at the same prefix.  The package provides
   the `XayaGameWasm::xayautil` and `XayaGameWasm::channelcore` imported
   targets and the `xaya_wasm_setup_target()` helper function.

The C++ source files are compiled **unchanged** from native.  The source
file lists and protobuf generation rules live in shared CMake fragment
files (`xayautil/sources.cmake`, `gamechannel/sources.cmake`,
`gamechannel/proto.cmake`) that are consumed by both the native and WASM
build systems, so the two always stay in sync automatically.

### Dependencies

All dependencies share a single install prefix (e.g. `/opt/wasm`):

| Library | How provided |
|---------|-------------|
| Protobuf (full, not lite) | Cross-compiled static WASM library |
| OpenSSL | Cross-compiled static WASM library |
| jsoncpp | Cross-compiled static WASM library |
| eth-utils + secp256k1 | Cross-compiled static WASM library |
| zlib | Emscripten built-in port (`-sUSE_ZLIB=1`) |

Full protobuf (not lite) is required because `protoversion.cpp` uses
the reflection API (`GetReflection`, `GetUnknownFields`).  zlib does not
need to be cross-compiled separately; it is provided by Emscripten's
built-in port.

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
macros (e.g. `#define FATAL 3`) because this conflicts with protobuf's
internal logging which uses token pasting (`LOGLEVEL_##LEVEL`).

The shim headers are installed into the prefix alongside the libraries so
that game projects do not need a copy of the libxayagame source tree.

## Prerequisites

1. **Emscripten SDK** — Install and activate (`source emsdk_env.sh`)
2. **Protobuf for WASM** — Full protobuf compiled as a static WASM library
3. **OpenSSL for WASM** — OpenSSL compiled as a static WASM library
4. **jsoncpp for WASM** — jsoncpp compiled as a static WASM library
5. **eth-utils for WASM** — eth-utils (with secp256k1) compiled as a static WASM library

All of the above should be installed under a single prefix (e.g. `/opt/wasm`),
which is also where libxayagame's WASM libraries will be installed.

## Building and installing the WASM libraries

```bash
emcmake cmake -B wasm-build wasm/ \
  -DCMAKE_INSTALL_PREFIX=/opt/wasm \
  -DWASM_DIR=/opt/wasm
cmake --build wasm-build
cmake --install wasm-build
```

This installs into `/opt/wasm`:
- `lib/libxayautil.a`, `lib/libchannelcore.a`
- `include/xayautil/`, `include/gamechannel/` (headers + generated proto headers)
- `include/glog/` (glog shim, so game projects need no source tree reference)
- `lib/cmake/XayaGameWasm/` (CMake package config)

## Usage in a game project

### 1. Generate game-specific protobuf stubs

The gamechannel proto stubs are already compiled into `channelcore`.
Games only need to generate stubs for their own game-specific protos:

```bash
protoc --cpp_out=generated/ -Iproto/ boardstate.proto boardmove.proto
```

### 2. Write CMakeLists.txt

```cmake
cmake_minimum_required (VERSION 3.14)
project (my-game-wasm LANGUAGES CXX)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

find_package (XayaGameWasm REQUIRED)

# Game-specific generated protobuf stubs.
set (GAME_PROTO_SOURCES
  generated/boardstate.pb.cc
)

add_executable (my-game
  my_board.cpp
  my_channel.cpp
  ${GAME_PROTO_SOURCES}
  bindings/wasm_bindings.cpp
)

# Include path for game-specific generated proto headers.
target_include_directories (my-game PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/generated"
)

xaya_wasm_setup_target (my-game createMyGameModule)
```

### 3. Build

```bash
emcmake cmake -B build -DCMAKE_PREFIX_PATH=/opt/wasm
cmake --build build
```

Output: `my-game.js` + `my-game.wasm`

### 4. Embind bindings

Each game writes its own Emscripten embind bindings to expose
game-specific C++ classes and functions to JavaScript.  Games should
use `ChannelManager` and other `channelcore` classes directly via
embind rather than reimplementing their logic in JavaScript.

## Provided targets and functions (after `find_package`)

| Name | Type | Description |
|------|------|-------------|
| `XayaGameWasm::xayautil` | imported target | Static xayautil WASM library |
| `XayaGameWasm::channelcore` | imported target | Static channelcore WASM library (links xayautil) |
| `xaya_wasm_setup_target (TARGET EXPORT)` | function | Configures Emscripten link/compile flags and links all dependencies |
