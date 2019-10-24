# Xaya Game Library

`libxayagame` is a C++ library that makes it easy to implement games on the
[Xaya platform](https://xaya.io/).  It takes care of the interaction with
the Xaya Core daemon, so that game developers only have to implement the
rules of their game.

[`mover`](mover/README.md) is a simple game using this library, where players
can move around an infinite plane.  It is fully functional, although mainly
meant as example and/or basis for more complex games.

This repository also contains a framework for [**game
channels**](http://www.ledgerjournal.org/ojs/index.php/ledger/article/view/15)
as well as [Xayaships](ships/README.md), which is an example game for
channels.

## Building

To build `libxayagame` and the example mover game, use the standard routine
for building autotools-based software:

```autogen.sh && ./configure && make```

After a successful build, you can optionally run `make check` and/or
`make install` to run tests and install the library and `moverd` on
your system, respectively.

### Prerequisites

`libxayagame` has a couple of dependencies which need to be installed
for the configuration and/or build to be successful:

- [`libjsoncpp`](https://github.com/open-source-parsers/jsoncpp):
  The Debian package `libjsoncpp-dev` can be used.
- [`jsonrpccpp`](https://github.com/cinemast/libjson-rpc-cpp/):
  For Debian, the packages `libjsonrpccpp-dev` and `libjsonrpccpp-tools`
  are not fresh enough.  They need to be built from source;
  in particular, it must be a version that includes the commit
  [`4f24acaf4c6737cd07d40a02edad0a56147e0713`](https://github.com/cinemast/libjson-rpc-cpp/commit/4f24acaf4c6737cd07d40a02edad0a56147e0713).
- [`ZeroMQ C++ bindings`](http://zeromq.org/bindings:cpp):
  Available in the Debian package `libzmq3-dev`.
- [`zlib`](https://zlib.net):
  Available in Debian as `zlib1g-dev`.
- [SQLite3](https://www.sqlite.org/) with the
  [session extension](https://www.sqlite.org/sessionintro.html).
  In Debian, the `libsqlite3-dev` package can be installed.
  Alternatively, build from source and configure with `--enable-session`.
- [LMDB](https://symas.com/lmdb):  Available for Debian in the
  `liblmdb-dev` package.
- [`glog`](https://github.com/google/glog):
  Available for Debian as `libgoogle-glog-dev`.
- [`gflags`](https://github.com/gflags/gflags):
  The package (`libgflags-dev`) included with Debian can be used.
- [Protocol buffers]((https://developers.google.com/protocol-buffers/)
  are used both in C++ and Python.  On Debian, the packages
  `libprotobuf-dev`, `protobuf-compiler` and `python-protobuf` can be used.

For the unit tests, also the
[Google test framework](https://github.com/google/googletest) is needed.
The package included with Debian 10 "Buster" is not fresh enough,
it should be built and installed from source instead.
