lib_LTLIBRARIES = libxayautil.la
xayautildir = $(includedir)/xayautil

pkgconfig_DATA = libxayautil.pc

libxayautil_la_CXXFLAGS = $(ZLIB_CFLAGS) $(OPENSSL_CFLAGS) $(GLOG_CFLAGS)
libxayautil_la_LIBADD = $(ZLIB_LIBS) $(OPENSSL_LIBS) $(GLOG_LIBS)
libxayautil_la_SOURCES = \
  base64.cpp \
  compression.cpp \
  cryptorand.cpp \
  hash.cpp \
  random.cpp \
  uint256.cpp
xayautil_HEADERS = \
  base64.hpp \
  compression.hpp \
  cryptorand.hpp \
  hash.hpp \
  random.hpp random.tpp \
  uint256.hpp
noinst_HEADERS = \
  compression_internal.hpp

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = $(ZLIB_CFLAGS) $(GLOG_CFLAGS) $(GTEST_CFLAGS)
tests_LDADD = $(builddir)/libxayautil.la \
  $(ZLIB_LIBS) $(GLOG_LIBS) $(GTEST_LIBS)
tests_SOURCES = \
  base64_tests.cpp \
  compression_tests.cpp \
  cryptorand_tests.cpp \
  hash_tests.cpp \
  random_tests.cpp \
  uint256_tests.cpp
