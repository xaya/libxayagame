# Builds a Docker image that has libxayagame (and all its dependencies)
# installed into /usr/local.  This can then be used as basis for images
# that build (and run) e.g. GSPs.

# Start by setting up a base image with all packages that we need
# both for the build but also in the final image.  These are the dependencies
# that are required as dev packages also for using libxayagame.
FROM alpine AS base
RUN apk add --no-cache \
  curl-dev \
  lmdb-dev \
  libmicrohttpd-dev \
  protobuf-dev \
  python3 \
  zlib-dev \
  czmq-dev

# Create the image that we use to build everything, and install additional
# packages that are needed only for the build itself.
FROM base AS build
RUN apk add --no-cache \
  autoconf \
  autoconf-archive \
  automake \
  build-base \
  cmake \
  git \
  gflags-dev \
  libtool \
  pkgconfig

# Build and install libargtable2 from source, which is not available
# as Alpine package.
ARG ARGTABLE_VERSION="2-13"
WORKDIR /usr/src
RUN wget http://prdownloads.sourceforge.net/argtable/argtable2-13.tar.gz
RUN tar zxvf argtable${ARGTABLE_VERSION}.tar.gz
WORKDIR /usr/src/argtable${ARGTABLE_VERSION}
RUN ./configure && make && make install-strip

# Build and install jsoncpp from source.  We need at least version >= 1.7.5,
# which includes an important fix for JSON parsing in some GSPs.
ARG JSONCPP_VERSION="1.8.4"
WORKDIR /usr/src/jsoncpp
RUN git clone -b ${JSONCPP_VERSION} \
  https://github.com/open-source-parsers/jsoncpp .
RUN cmake . \
  -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF
RUN make && make install/strip

# We need to install libjson-rpc-cpp from source.
WORKDIR /usr/src/libjson-rpc-cpp
RUN git clone https://github.com/cinemast/libjson-rpc-cpp .
RUN cmake . \
  -DREDIS_SERVER=NO -DREDIS_CLIENT=NO \
  -DCOMPILE_TESTS=NO -DCOMPILE_EXAMPLES=NO \
  -DWITH_COVERAGE=NO
RUN make && make install/strip

# Install glog from source.
WORKDIR /usr/src/glog
RUN git clone https://github.com/google/glog .
RUN cmake . && make && make install/strip

# We also need to install googletest from source.
WORKDIR /usr/src/googletest
RUN git clone https://github.com/google/googletest .
RUN cmake . && make && make install/strip

# The ZMQ C++ bindings need to be installed from source.
ARG CPPZMQ_VERSION="4.6.0"
WORKDIR /usr/src/cppzmq
RUN git clone -b v${CPPZMQ_VERSION} https://github.com/zeromq/cppzmq .
RUN cp zmq.hpp /usr/local/include

# Build and install sqlite3 from source with the session extension
# enabled as needed.
ARG SQLITE_VERSION="3310100"
WORKDIR /usr/src
RUN wget https://www.sqlite.org/2020/sqlite-autoconf-${SQLITE_VERSION}.tar.gz
RUN tar zxvf sqlite-autoconf-${SQLITE_VERSION}.tar.gz
WORKDIR /usr/src/sqlite-autoconf-${SQLITE_VERSION}
RUN ./configure CFLAGS="-DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK"
RUN make && make install-strip

# Also add a utility script for copying dynamic libraries needed for
# a given binary.  This can be used by GSP images based on this one
# to make them as minimal as possible.
WORKDIR /usr/src/scripts
RUN git clone https://github.com/hemanth/futhark .
RUN cp sh/cpld.bash /usr/local/bin/cpld
RUN chmod a+x /usr/local/bin/cpld

# Make sure all installed dependencies are visible.
ENV PKG_CONFIG_PATH "/usr/local/lib64/pkgconfig"
ENV LD_LIBRARY_PATH "/usr/local/lib:/usr/local/lib64"
RUN echo $LD_LIBRARY_PATH

# Build and install libxayagame itself.  Make sure to clean out any
# potential garbage copied over in the build context.
WORKDIR /usr/src/libxayagame
COPY . .
RUN make distclean || true
RUN ./autogen.sh && ./configure && make && make install-strip

# For the final image, just copy over all built / installed stuff and
# add in the non-dev libraries needed (where we installed the dev version
# on the builder package only).  We also add bash for the cpld script.
FROM base
COPY --from=build /usr/local /usr/local/
ENV PKG_CONFIG_PATH "/usr/local/lib64/pkgconfig"
ENV LD_LIBRARY_PATH "/usr/local/lib:/usr/local/lib64"
RUN apk add --no-cache \
  bash \
  gflags
LABEL description="Development image with libxayagame and dependencies"
