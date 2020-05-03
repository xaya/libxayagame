# Builds a Docker image that has libxayagame (and all its dependencies)
# installed into /usr/local.  This can then be used as basis for images
# that build (and run) e.g. GSPs.

# Start by setting up a base image with Debian packages that we need
# both for the build but also in the final image.  These are the dependencies
# that are required as dev packages also for using libxayagame.
FROM debian:buster AS basepackages
RUN apt-get update && apt-get install -y \
  libgoogle-glog-dev \
  liblmdb-dev \
  libprotobuf-dev \
  libsqlite3-dev \
  libssl-dev \
  libzmq3-dev \
  python3 \
  python3-jsonrpclib-pelix \
  python3-protobuf \
  zlib1g-dev

# Create the image that we use to build everything, and install additional
# packages that are needed only for the build itself.
FROM basepackages AS build
RUN apt-get install -y \
  autoconf \
  autoconf-archive \
  automake \
  build-essential \
  cmake \
  git \
  libargtable2-dev \
  libcurl4-openssl-dev \
  libgflags-dev \
  libmicrohttpd-dev \
  libtool \
  pkg-config \
  protobuf-compiler

# Build and install jsoncpp from source.  This is required at least until
# a version >= 1.7.5 is part of the base image.  That version includes
# an important fix for JSON parsing in some GSPs.
WORKDIR /usr/src/jsoncpp
RUN git clone -b 1.8.4 https://github.com/open-source-parsers/jsoncpp .
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

# We also need to install googletest from source.
WORKDIR /usr/src/googletest
RUN git clone https://github.com/google/googletest .
RUN cmake . && make && make install/strip

# The ZMQ C++ bindings need to be installed from source.
WORKDIR /usr/src/cppzmq
RUN git clone -b v4.6.0 https://github.com/zeromq/cppzmq .
RUN cp zmq.hpp /usr/local/include

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
RUN ./autogen.sh && ./configure && make && make install-strip

# For the final image, just copy over all built / installed stuff and
# add in the non-dev libraries needed (where we installed the dev version
# on the builder package only).
FROM basepackages
COPY --from=build /usr/local /usr/local
RUN apt-get install -y \
  libargtable2-0 \
  libcurl4 \
  libgflags2.2 \
  libmicrohttpd12
LABEL description="Debian-based image that includes libxayagame and dependencies prebuilt and installed."
