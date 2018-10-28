# Xaya Game Library and Mover

`libxayagame` is a C++ library that makes it easy to implement games on the
[Xaya platform](https://xaya.io/).  It takes care of the interaction with
the Xaya Core daemon, so that game developers only have to implement the
rules of their game.

[`mover`](mover/README.md) is a simple game using this library, where players
can move around an infinite plane.  It is fully functional, although mainly
meant as example and/or basis for more complex games.

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
  Available for Debian-based operating systems in the `libjsoncpp-dev`
  package.
- [`jsonrpccpp`](https://github.com/cinemast/libjson-rpc-cpp/):
  For Debian-based systems, install `libjsonrpccpp-dev`
  and `libjsonrpccpp-tools`.
- [`ZeroMQ C++ bindings`](http://zeromq.org/bindings:cpp):
  Available in the Debian package `libzmq3-dev`.
- [SQLite3](https://www.sqlite.org/) with the
  [session extension](https://www.sqlite.org/sessionintro.html).
  In Debian, the `libsqlite3-dev` package can be installed.
  Alternatively, build from source and configure with `--enable-session`.
- [LMDB](https://symas.com/lmdb):  Available for Debian in the
  `liblmdb-dev` package.
- [`glog`](https://github.com/google/glog):
  Available for Debian as `libgoogle-glog-dev`.
- [`gflags`](https://github.com/gflags/gflags):
  The package (`libgflags-dev`) included with Debian 9 "Stretch" does
  not include the `pkg-config` file yet, build from source instead.

For the unit tests, also the
[Google test framework](https://github.com/google/googletest) is needed.
The package included with Debian 9 "Stretch" is not fresh enough,
it should be built and installed from source instead.

The [mover](mover/README.md) example game also needs
[protocol buffers](https://developers.google.com/protocol-buffers/).
On Debian, install `libprotobuf-dev` and `protobuf-compiler`.
