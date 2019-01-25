AC_INIT([libxayagame], [0.1])
AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX
AM_PROG_AR
AM_PATH_PYTHON
PKG_INSTALLDIR

AC_LANG([C++])
LT_INIT

AX_CXX_COMPILE_STDCXX([14], [noext])
AX_CHECK_COMPILE_FLAG([-Wall], [CXXFLAGS+=" -Wall"])
AX_CHECK_COMPILE_FLAG([-Werror], [CXXFLAGS+=" -Werror"])
AX_CHECK_COMPILE_FLAG([-pedantic], [CXXFLAGS+=" -pedantic"])
AX_CHECK_COMPILE_FLAG([-Wno-deprecated], [CXXFLAGS+=" -Wno-deprecated"])

# Windows defines ERROR, which requires us to tell glog to not define
# it as abbreviated log severity (LOG(ERROR) still works, though, and
# that is all that we actually use in the code).
# See https://hpc.nih.gov/development/glog.html.
CXXFLAGS+=" -DGLOG_NO_ABBREVIATED_SEVERITIES"

# Public dependencies (exposed in the headers of libxayagame to
# users of the library).
AX_PKG_CHECK_MODULES([JSONCPP], [jsoncpp], [])
AX_PKG_CHECK_MODULES([JSONRPCCLIENT], [libjsonrpccpp-client], [])
AX_PKG_CHECK_MODULES([JSONRPCSERVER], [libjsonrpccpp-server], [])
AX_PKG_CHECK_MODULES([SQLITE3], [sqlite3], [],
  [SQLITE3_CFLAGS+=" -DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK"])
AX_PKG_CHECK_MODULES([LMDB], [lmdb], [])

# Private dependencies of the library itself.
AX_PKG_CHECK_MODULES([GLOG], [], [libglog])
AX_PKG_CHECK_MODULES([OPENSSL], [], [openssl])
AX_PKG_CHECK_MODULES([ZMQ], [], [libzmq])

# Private dependencies that are not needed for libxayagame, but only for
# the unit tests of the moverd binary.
PKG_CHECK_MODULES([GFLAGS], [gflags])
PKG_CHECK_MODULES([GTEST], [gmock gtest_main])
PKG_CHECK_MODULES([PROTOBUF], [protobuf])

AC_CONFIG_FILES([
  Makefile \
  xayagame/Makefile \
  xayagametest/Makefile \
  mover/Makefile \
  mover/gametest/Makefile \
  \
  xayagame/libxayagame.pc
])
AC_OUTPUT

echo
echo "CXXFLAGS: ${CXXFLAGS}"
