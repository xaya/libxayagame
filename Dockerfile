# Builds a Docker image that has libxayagame (and all its dependencies)
# installed into /usr/local.  This can then be used as basis for images
# that build (and run) e.g. GSPs.

# Start by setting up a base image with Debian packages that we need
# both for the build but also in the final image.
FROM debian:buster AS basepackages
RUN apt-get update && apt-get install -y \
  build-essential \
  libargtable2-dev \
  libcurl4-openssl-dev \
  libgflags-dev \
  libgoogle-glog-dev \
  liblmdb-dev \
  libmicrohttpd-dev \
  libprotobuf-dev \
  libssl-dev \
  libsqlite3-dev \
  libzmq3-dev \
  python2 \
  python-protobuf \
  python-jsonrpclib \
  zlib1g-dev

# Create the image that we use to build everything, and install additional
# packages that are needed only for the build itself.
FROM basepackages AS build
RUN apt-get update && apt-get install -y \
  autoconf \
  autoconf-archive \
  automake \
  cmake \
  git \
  libtool \
  pkg-config \
  protobuf-compiler

# Build and install jsoncpp from source.  This is required at least until
# a version >= 1.7.5 is part of the base image.  That version includes
# an important fix for JSON parsing in some GSPs.
WORKDIR /usr/src/jsoncpp
RUN git clone -b 1.8.4 https://github.com/open-source-parsers/jsoncpp .
RUN cmake . \
  -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF \
  && make && make install

# We need to install libjson-rpc-cpp from source.
WORKDIR /usr/src/libjson-rpc-cpp
# FIXME: Check out "master" branch instead once
# https://github.com/cinemast/libjson-rpc-cpp/pull/288 is in it.
RUN git clone -b develop https://github.com/cinemast/libjson-rpc-cpp .
RUN cmake . \
  -DREDIS_SERVER=NO -DREDIS_CLIENT=NO \
  -DCOMPILE_TESTS=NO -DCOMPILE_EXAMPLES=NO \
  -DWITH_COVERAGE=NO \
  && make && make install

# We also need to install googletest from source.
WORKDIR /usr/src/googletest
RUN git clone https://github.com/google/googletest .
RUN cmake . && make && make install

# The ZMQ C++ bindings need to be installed from source.
WORKDIR /usr/src/cppzmq
RUN git clone -b v4.6.0 https://github.com/zeromq/cppzmq .
RUN cp zmq.hpp /usr/local/include

# Make sure all installed dependencies are visible.
RUN ldconfig

# Build and install libxayagame itself.
WORKDIR /usr/src/libxayagame
COPY . .
RUN ./autogen.sh && ./configure && make && make install

# For the final image, just copy over all built / installed stuff.
FROM basepackages
COPY --from=build /usr/local /usr/local
LABEL description="Debian-based image that includes libxayagame and dependencies prebuilt and installed."
