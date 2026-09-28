# Start by setting up a base image with all packages that we need
# both for the build but also in the final image.  These are the dependencies
# that are required as dev packages also for using libxayagame.
FROM debian:13-slim AS base
RUN apt -y update && apt -y install --no-install-recommends \
  cppzmq-dev \
  libargtable2-dev \
  libzmq3-dev \
  zlib1g-dev \
  liblmdb-dev \
  libcurl4-openssl-dev \
  libssl-dev \
  libmicrohttpd-dev \
  libgoogle-glog-dev \
  libgflags-dev \
  libprotobuf-dev \
  libsecp256k1-dev \
  protobuf-compiler \
  python3 \
  python3-protobuf \
  python3-venv

# Create the image that we use to build everything, and install additional
# packages that are needed only for the build itself.
FROM base AS build
RUN apt -y update && apt -y install --no-install-recommends \
  autoconf \
  autoconf-archive \
  automake \
  build-essential \
  ca-certificates \
  cmake \
  git \
  libtool \
  pkg-config

# Number of parallel cores to use for make builds.
ARG N=1

# Build and install sqlite3 with custom flags.
ARG SQLITE_VERSION="version-3.52.0"
WORKDIR /usr/src/sqlite3
RUN git clone https://github.com/sqlite/sqlite . \
  && git checkout "${SQLITE_VERSION}" \
  && ./configure --enable-session CFLAGS="-DSQLITE_ENABLE_SNAPSHOT" \
  && make -j${N} \
  && make install

# Install a pinned and well-defined version of jsoncpp.  We use that library
# to parse data also in consensus-relevant ways (such as UncompressJson),
# and while we try to ensure parsing is strict, pinning the version is
# some additional safe-guard against potential bugs or consensus changes.
ARG JSONCPP_VERSION="1.9.8"
WORKDIR /usr/src/jsoncpp
RUN git clone https://github.com/open-source-parsers/jsoncpp . \
  && git checkout ${JSONCPP_VERSION} \
  && cmake -B build -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build -j${N} \
  && cmake --install build --strip

# We need to install libjson-rpc-cpp from source.
# The official repository in v1.4.1 has a bug, we need to use
# the fixed version (until it gets merged eventually).
ARG JSONRPCCPP_VERSION="24c069f74656ef9994623c5aecfabd5edcd85681"
WORKDIR /usr/src/libjson-rpc-cpp
RUN git clone https://github.com/domob1812/libjson-rpc-cpp . \
  && git checkout ${JSONRPCCPP_VERSION} \
  && cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DREDIS_SERVER=NO -DREDIS_CLIENT=NO \
      -DCOMPILE_TESTS=NO -DCOMPILE_EXAMPLES=NO \
      -DWITH_COVERAGE=NO \
  && cmake --build build -j${N} && cmake --install build --strip

# We also need to install googletest from source.
WORKDIR /usr/src/googletest
RUN git clone https://github.com/google/googletest .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build -j${N} \
  && cmake --install build --strip

# Build and install eth-utils.
ARG ETHUTILS_COMMIT="ece89a90a62f406deeb8dc25534b94fc091438f4"
WORKDIR /usr/src/ethutils
RUN git clone https://github.com/xaya/eth-utils . \
  && git checkout ${ETHUTILS_COMMIT}
RUN ./autogen.sh && ./configure && make -j${N} && make install-strip

# Also add a utility script for copying dynamic libraries needed for
# a given binary.  This can be used by GSP images based on this one
# to make them as minimal as possible.
WORKDIR /usr/src/scripts
RUN git clone https://github.com/hemanth/futhark .
RUN cp sh/cpld.bash /usr/local/bin/cpld
RUN chmod a+x /usr/local/bin/cpld

# Make sure all installed dependencies are visible.
RUN ldconfig

# Build and install libxayagame itself.  Make sure to clean out any
# potential garbage copied over in the build context.
WORKDIR /usr/src/libxayagame
COPY . .
RUN rm -rf build \
  && cmake -B build -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build -j${N} \
  && cmake --install build --strip

# For the final image, just copy over all built / installed stuff and
# add in the non-dev libraries needed (where we installed the dev version
# on the builder image only).  We also add bash for the cpld script.
FROM base
COPY --from=build /usr/local /usr/local/
ENV PKG_CONFIG_PATH="/usr/local/lib64/pkgconfig"
ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib64"
RUN apt -y update && apt -y install --no-install-recommends \
  bash
LABEL description="Development image with libxayagame and dependencies"
