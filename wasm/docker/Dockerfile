# This Dockerfile builds an image that has the game-channel libraries
# and all required dependencies cross-compiled to WASM for use by
# game projects.

FROM emscripten/emsdk

RUN apt-get update && apt-get install -y \
  autoconf \
  autoconf-archive \
  cmake \
  g++ \
  libtool \
  perl \
  pkg-config

# Number of cores to use for parallel builds.
ARG N=1

# Prefix used by emsdk for compiled artefacts.  We use the sysroot root
# directly (not the /usr sub-prefix) so that pkg-config files are installed
# into sysroot/lib/pkgconfig, which is the path emscripten's pkg-config
# wrapper searches by default.
ENV WASM_PREFIX="/emsdk/upstream/emscripten/cache/sysroot"

# Install the glog shim into the sysroot so all subsequent builds that
# do #include <glog/logging.h> get the stub without needing extra flags.
# NOTE: COPY does not expand ENV variables, so the sysroot path is hardcoded.
COPY wasm/shims/glog/logging.h /emsdk/upstream/emscripten/cache/sysroot/include/glog/logging.h
COPY wasm/docker/libglog.pc /emsdk/upstream/emscripten/cache/sysroot/lib/pkgconfig/libglog.pc

# Build and install OpenSSL.
# OpenSSL uses its own custom Perl-based Configure (not autoconf), so the
# build procedure differs from other autotools libraries:
#
#  1. We call emconfigure ./Configure with an explicit arch of linux-x32
#     (32-bit pointer model, matching WASM32).  emconfigure injects CC=emcc
#     etc. into the environment which OpenSSL's Configure script picks up.
#
#  2. After Configure runs it writes CROSS_COMPILE=<emscripten-path> into the
#     generated Makefile, which would cause path-doubling bugs when make
#     expands $(CROSS_COMPILE)ar etc.  The sed clears that variable.
#
#  3. We build only the library targets explicitly; the default 'all' target
#     would attempt to link executables that cannot run on WASM.
#
#  4. 'make install_sw' installs headers, static libs, and .pc files without
#     any documentation or command-line tools.
ARG OPENSSL_VERSION="openssl-3.6.1"
WORKDIR /src/openssl
RUN git clone https://github.com/openssl/openssl . \
    && git checkout "${OPENSSL_VERSION}" \
    && emconfigure ./Configure linux-x32 \
          --prefix="${WASM_PREFIX}" \
          --libdir=lib \
          --cross-compile-prefix= \
          no-asm \
          no-shared \
          no-dso \
          no-engine \
          no-module \
          no-sse2 \
          no-threads \
          no-apps \
          no-tests \
    && sed -i 's|^CROSS_COMPILE.*$|CROSS_COMPILE=|' Makefile \
    && emmake make -j${N} build_generated libssl.a libcrypto.a \
    && emmake make install_sw \
    && make distclean

# Build protoc (native) and libprotobuf (WASM) from the same source checkout.
# The native build provides the protoc code-generator binary that runs at
# cmake build time; the WASM build provides the headers and static library
# for linking.  Building both from the same checkout guarantees that the
# protoc version and the library version are identical.
ARG PROTOBUF_VERSION="v3.21.12"
WORKDIR /src/protobuf
RUN git clone https://github.com/protocolbuffers/protobuf . \
    && git checkout "${PROTOBUF_VERSION}" \
    \
    && cmake -B build-native \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          -Dprotobuf_BUILD_TESTS=OFF \
          -Dprotobuf_BUILD_SHARED_LIBS=OFF \
    && cmake --build build-native -j${N} --target protoc \
    && cp build-native/protoc /usr/local/bin/protoc \
    \
    && emcmake cmake -B build-wasm \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="${WASM_PREFIX}" \
          -Dprotobuf_BUILD_TESTS=OFF \
          -Dprotobuf_BUILD_PROTOC_BINARIES=OFF \
          -Dprotobuf_BUILD_SHARED_LIBS=OFF \
    && cmake --build build-wasm -j${N} \
    && cmake --install build-wasm \
    && rm -rf build-native build-wasm

# Build and install jsoncpp.
ARG JSONCPP_VERSION="1.9.6"
WORKDIR /src/jsoncpp
RUN git clone https://github.com/open-source-parsers/jsoncpp . \
    && git checkout "${JSONCPP_VERSION}" \
    && emcmake cmake -B build . \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="${WASM_PREFIX}" \
          -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON \
          -DJSONCPP_WITH_TESTS=OFF \
    && cmake --build build -j${N} \
    && cmake --install build \
    && rm -rf build

# Build and install secp256k1.
ARG SECP256K1_VERSION="v0.7.1"
WORKDIR /src/secp256k1
RUN git clone https://github.com/bitcoin-core/secp256k1 . \
    && git checkout "${SECP256K1_VERSION}" \
    && ./autogen.sh \
    && emconfigure ./configure \
          --prefix="${WASM_PREFIX}" \
          --host=wasm32-unknown-emscripten \
          --disable-shared --enable-static \
          --enable-module-recovery \
          --disable-benchmark --disable-exhaustive-tests --disable-tests \
    && emmake make -j${N} \
    && emmake make install \
    && emmake make distclean

# Build and install eth-utils.
ARG ETHUTILS_VERSION="31c8936812878b4a717feefb2ffc8429ced8d9c6"
WORKDIR /src/eth-utils
RUN git clone https://github.com/xaya/eth-utils . \
    && git checkout "${ETHUTILS_VERSION}" \
    && ./autogen.sh \
    && emconfigure ./configure \
          --prefix="${WASM_PREFIX}" \
          --host=wasm32-unknown-emscripten \
          --disable-shared --enable-static \
          --disable-tests \
    && emmake make -j${N} \
    && emmake make install \
    && emmake make distclean

# Build and install libxayagame itself (WASM parts).
COPY . /src/libxayagame/
WORKDIR /src/libxayagame
RUN emcmake cmake -B build wasm \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${WASM_PREFIX}" \
        -DWASM_DIR="${WASM_PREFIX}" \
    && cmake --build build -j${N} \
    && cmake --install build \
    && rm -rf build
