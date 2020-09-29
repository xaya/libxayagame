AC_INIT([libxayagame], [0.1])
AM_INIT_AUTOMAKE([subdir-objects])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES([yes])

AC_PROG_CXX
AM_PROG_AR
AM_PATH_PYTHON([3.7])
PKG_INSTALLDIR

AC_LANG([C++])
LT_INIT

AX_CXX_COMPILE_STDCXX([14], [noext])
AX_CHECK_COMPILE_FLAG([-Wall], [CXXFLAGS="${CXXFLAGS} -Wall"])
AX_CHECK_COMPILE_FLAG([-Werror], [CXXFLAGS="${CXXFLAGS} -Werror"])
AX_CHECK_COMPILE_FLAG([-pedantic], [CXXFLAGS="${CXXFLAGS} -pedantic"])
AX_CHECK_COMPILE_FLAG([-Wno-deprecated],
                      [CXXFLAGS="${CXXFLAGS} -Wno-deprecated"])

# Windows defines ERROR, which requires us to tell glog to not define
# it as abbreviated log severity (LOG(ERROR) still works, though, and
# that is all that we actually use in the code).
# See https://hpc.nih.gov/development/glog.html.
CXXFLAGS="${CXXFLAGS} -DGLOG_NO_ABBREVIATED_SEVERITIES"

# Public dependencies (exposed in the headers of libxayagame to
# users of the library).
# jsoncpp 1.7.5 first included a fix for
# https://github.com/open-source-parsers/jsoncpp/issues/511.  We need this,
# since we use it to enforce rules on when uncompressing JSON values with
# xayautil.
AX_PKG_CHECK_MODULES([JSONCPP], [jsoncpp >= 1.7.5], [])
AX_PKG_CHECK_MODULES([JSONRPCCLIENT], [libjsonrpccpp-client], [])
AX_PKG_CHECK_MODULES([JSONRPCSERVER], [libjsonrpccpp-server], [])
AX_PKG_CHECK_MODULES([SQLITE3], [sqlite3], [],
  [SQLITE3_CFLAGS="${SQLITE3_CFLAGS} -DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK"])
AX_PKG_CHECK_MODULES([LMDB], [lmdb], [])
AX_PKG_CHECK_MODULES([PROTOBUF], [protobuf])
AX_PKG_CHECK_MODULES([GLOG], [libglog])

# Private dependencies of the library itself.
AX_PKG_CHECK_MODULES([OPENSSL], [], [openssl])
AX_PKG_CHECK_MODULES([MHD], [], [libmicrohttpd])
# We use deflateGetDictionary from zlib, which was introduced in version
# 1.2.9.  Since zlib itself recommends using 1.2.11 over .9 and .10, let's
# make sure we have at least this version.
AX_PKG_CHECK_MODULES([ZLIB], [], [zlib >= 1.2.11])
# We use the recv variant taking message_t& over deprecated older functions,
# which requires at least version 4.3.1.
AX_PKG_CHECK_MODULES([ZMQ], [], [libzmq >= 4.3.1])

# Private dependencies that are not needed for libxayagame, but only for
# the unit tests and the example binaries.
PKG_CHECK_MODULES([GFLAGS], [gflags])
PKG_CHECK_MODULES([GTEST], [gmock gtest_main])

AC_CONFIG_FILES([
  Makefile \
  gamechannel/Makefile \
  mover/Makefile \
  mover/gametest/Makefile \
  nonfungible/Makefile \
  nonfungible/gametest/Makefile \
  ships/Makefile \
  ships/channeltest/Makefile \
  ships/gametest/Makefile \
  xayagame/Makefile \
  xayagametest/Makefile \
  xayautil/Makefile \
  \
  xayagame/libxayagame.pc \
  xayautil/libxayautil.pc
])
AC_OUTPUT

echo
echo "CXXFLAGS: ${CXXFLAGS}"
