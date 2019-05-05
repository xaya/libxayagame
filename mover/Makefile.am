SUBDIRS = gametest

noinst_LTLIBRARIES = libmover.la
bin_PROGRAMS = moverd

EXTRA_DIST = proto/mover.proto

BUILT_SOURCES = proto/mover.pb.h
CLEANFILES = proto/mover.pb.h proto/mover.pb.cc

libmover_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(PROTOBUF_CFLAGS)
libmover_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS) $(PROTOBUF_LIBS)
libmover_la_SOURCES = \
  logic.cpp \
  proto/mover.pb.cc
noinst_HEADERS = \
  logic.hpp \
  proto/mover.pb.h

moverd_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) \
  $(GFLAGS_CFLAGS) $(PROTOBUF_CFLAGS)
moverd_LDADD = \
  $(builddir)/libmover.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(GFLAGS_LIBS) $(PROTOBUF_LIBS)
moverd_SOURCES = main.cpp

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(PROTOBUF_CFLAGS) $(GTEST_CFLAGS)
tests_LDADD = $(builddir)/libmover.la \
  $(JSONCPP_LIBS) $(GLOB_LIBS) $(PROTOBUF_LIBS) $(GTEST_LIBS)
tests_SOURCES = \
  logic_tests.cpp

proto/mover.pb.h proto/mover.pb.cc: $(srcdir)/proto/mover.proto
	protoc --cpp_out=. "$<"
