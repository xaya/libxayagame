lib_LTLIBRARIES = libxayautil.la
xayautildir = $(includedir)/xayautil

pkgconfig_DATA = libxayautil.pc

libxayautil_la_CXXFLAGS = $(OPENSSL_CFLAGS) $(GLOG_CFLAGS)
libxayautil_la_LIBADD = $(OPENSSL_LIBS) $(GLOG_LIBS)
libxayautil_la_SOURCES = \
  base64.cpp \
  hash.cpp \
  random.cpp \
  uint256.cpp
xayautil_HEADERS = \
  base64.hpp \
  hash.hpp \
  random.hpp \
  uint256.hpp

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = $(GLOG_CFLAGS) $(GTEST_CFLAGS)
tests_LDADD = $(builddir)/libxayautil.la \
  $(GLOG_LIBS) $(GTEST_LIBS)
tests_SOURCES = \
  base64_tests.cpp \
  hash_tests.cpp \
  random_tests.cpp \
  uint256_tests.cpp