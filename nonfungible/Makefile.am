noinst_LTLIBRARIES = libnonfungible.la
bin_PROGRAMS = nonfungibled

EXTRA_DIST = \
  schema.sql schema_head.cpp schema_tail.cpp

CLEANFILES = schema.cpp

libnonfungible_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(SQLITE_CFLAGS) $(GLOG_CFLAGS)
libnonfungible_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(SQLITE_LIBS) $(GLOG_LIBS)
libnonfungible_la_SOURCES = \
  assets.cpp \
  dbutils.cpp \
  logic.cpp \
  moveparser.cpp \
  moveprocessor.cpp \
  schema.cpp
noinst_HEADERS = \
  assets.hpp \
  dbutils.hpp \
  logic.hpp \
  moveparser.hpp \
  moveprocessor.hpp \
  schema.hpp

nonfungibled_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GFLAGS_CFLAGS)
nonfungibled_LDADD = \
  $(builddir)/libnonfungible.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(GLOG_LIBS) $(GFLAGS_LIBS)
nonfungibled_SOURCES = main.cpp

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(GTEST_CFLAGS) \
  $(JSONCPP_CFLAGS) $(SQLITE_CFLAGS) $(GLOG_CFLAGS)
tests_LDADD = \
  $(builddir)/libnonfungible.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(GTEST_LIBS) \
  $(JSONCPP_LIBS) $(SQLITE_LIBS) $(GLOG_LIBS)
tests_SOURCES = \
  assets_tests.cpp \
  moveprocessor_tests.cpp \
  schema_tests.cpp \
  \
  testutils.cpp
check_HEADERS = \
  testutils.hpp

schema.cpp: schema_head.cpp schema.sql schema_tail.cpp
	cat $^ >$@
