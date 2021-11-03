# Start by setting up a base image with all packages that we need
# both for the build but also in the final image.  These are the dependencies
# that are required as dev packages also for using libxayagame.
FROM ubuntu:22.04 AS base
RUN apt -y update
ENV TZ="Europe/London"
ARG DEBIAN_FRONTEND="noninteractive"
RUN apt update && apt -y install \
  libargtable2-dev \
  libzmq3-dev \
  zlib1g-dev \
  libsqlite3-dev \
  liblmdb-dev \
  libcurl4-openssl-dev \
  libssl-dev \
  libmicrohttpd-dev \
  libgoogle-glog-dev \
  libgflags-dev \
  libprotobuf-dev \
  protobuf-compiler \
  python3 \
  python3-protobuf

# Create the image that we use to build everything, and install additional
# packages that are needed only for the build itself.
FROM base AS build
RUN apt -y install \
  autoconf \
  autoconf-archive \
  automake \
  build-essential \
  cmake \
  git \
  libtool \
  pkg-config

# Number of parallel cores to use for make builds.
ARG N=1

# Build and install jsoncpp from source.  We need at least version >= 1.7.5,
# which includes an important fix for JSON parsing in some GSPs.
ARG JSONCPP_VERSION="1.9.5"
WORKDIR /usr/src/jsoncpp/build
RUN git clone -b ${JSONCPP_VERSION} \
  https://github.com/open-source-parsers/jsoncpp ../source
RUN cmake ../source \
  -DJSONCPP_WITH_PKGCONFIG_SUPPORT=ON \
  -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF
RUN make -j${N} && make install/strip

# We need to install libjson-rpc-cpp from source.
WORKDIR /usr/src/libjson-rpc-cpp
RUN git clone https://github.com/cinemast/libjson-rpc-cpp .
RUN cmake . \
  -DREDIS_SERVER=NO -DREDIS_CLIENT=NO \
  -DCOMPILE_TESTS=NO -DCOMPILE_EXAMPLES=NO \
  -DWITH_COVERAGE=NO
RUN make -j${N} && make install/strip

# We also need to install googletest from source.
WORKDIR /usr/src/googletest
RUN git clone https://github.com/google/googletest .
RUN cmake . && make -j${N} && make install/strip

# The ZMQ C++ bindings need to be installed from source.
ARG CPPZMQ_VERSION="4.8.1"
WORKDIR /usr/src/cppzmq
RUN git clone -b v${CPPZMQ_VERSION} https://github.com/zeromq/cppzmq .
RUN cp zmq.hpp /usr/local/include

# Build and install libsecp256k1.
ARG SECP256K1_VERSION="master"
WORKDIR /usr/src/libsecp256k1
RUN git clone -b ${SECP256K1_VERSION} \
  https://github.com/bitcoin-core/secp256k1 .
RUN ./autogen.sh \
    && ./configure --disable-tests --disable-benchmark  \
                   --enable-module-recovery \
    && make -j${N} && make install-strip

# Build and install eth-utils.
ARG ETHUTILS_VERSION="master"
WORKDIR /usr/src/ethutils
RUN git clone -b ${ETHUTILS_VERSION} \
  https://github.com/xaya/eth-utils .
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
RUN make distclean || true
RUN ./autogen.sh && ./configure && make -j${N} && make install-strip

# For the final image, just copy over all built / installed stuff and
# add in the non-dev libraries needed (where we installed the dev version
# on the builder image only).  We also add bash for the cpld script.
FROM base
COPY --from=build /usr/local /usr/local/
ENV PKG_CONFIG_PATH "/usr/local/lib64/pkgconfig"
ENV LD_LIBRARY_PATH "/usr/local/lib:/usr/local/lib64"
RUN apt -y install \
  bash
LABEL description="Development image with libxayagame and dependencies"
