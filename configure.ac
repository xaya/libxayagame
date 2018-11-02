AC_INIT([libxayagame], [0.1])
AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX
AM_PROG_AR

AC_CANONICAL_HOST

build_linux=no
build_windows=no
build_mac=no

# Detect the target system
case "${host_os}" in
    linux*)
        build_linux=yes
        ;;
    cygwin*|mingw*)
        build_windows=yes
        ;;
    darwin*)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
    *)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

# Pass the conditionals to automake
AM_CONDITIONAL([LINUX], [test "$build_linux" = "yes"])
AM_CONDITIONAL([WINDOWS], [test "$build_windows" = "yes"])
AM_CONDITIONAL([OSX], [test "$build_mac" = "yes"])

AC_LANG([C++])
LT_INIT

# Windows mu have no undefined symbol in dll, we need -no-undefined 
# flag to work, so lets trigger clean dll build

AM_COND_IF( [WINDOWS], [
LT_INIT([win32-dll])
])



AX_CXX_COMPILE_STDCXX([14], [noext])
AX_CHECK_COMPILE_FLAG([-Wall], [CXXFLAGS+=" -Wall"])
AX_CHECK_COMPILE_FLAG([-Werror], [CXXFLAGS+=" -Werror"])
AX_CHECK_COMPILE_FLAG([-pedantic], [CXXFLAGS+=" -pedantic"])



PKG_CHECK_MODULES([JSONCPP], [jsoncpp])
PKG_CHECK_MODULES([JSONRPCCLIENT], [libjsonrpccpp-client])
PKG_CHECK_MODULES([JSONRPCSERVER], [libjsonrpccpp-server])
PKG_CHECK_MODULES([GLOG], [libglog])
PKG_CHECK_MODULES([GFLAGS], [gflags])
PKG_CHECK_MODULES([GTEST], [gmock gtest_main])
PKG_CHECK_MODULES([PROTOBUF], [protobuf])
PKG_CHECK_MODULES([SQLITE3], [sqlite3],
  [SQLITE3_CFLAGS+=" -DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK"])
PKG_CHECK_MODULES([ZMQ], [libzmq])

# When cross-compile to windows, we don't need mover folder
# also we need to pass extra CPP flag for jsonrpccpp library
# CONDITIONAL START
  


AM_COND_IF( [WINDOWS], [
CXXFLAGS+=" -DGLOG_NO_ABBREVIATED_SEVERITIES"
AC_CONFIG_FILES([
      Makefile \
      xayagame/Makefile
])] )

AM_COND_IF( [LINUX], [
AC_CONFIG_FILES([
	  mover/Makefile \
	  mover/gametest/Makefile
])] )


# CONDITIONAL END


AC_OUTPUT

echo
echo "CXXFLAGS: ${CXXFLAGS}"
