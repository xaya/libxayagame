SUBDIRS = gametest

noinst_LTLIBRARIES = libships.la
bin_PROGRAMS = shipsd ships-channel

PROTOS = \
  proto/boardmove.proto \
  proto/boardstate.proto \
  proto/winnerstatement.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)
PROTOPY = $(PROTOS:.proto=_pb2.py)

EXTRA_DIST = $(PROTOS) \
  schema.sql schema_head.cpp schema_tail.cpp

BUILT_SOURCES = $(PROTOHEADERS) $(PROTOPY)
CLEANFILES = $(PROTOHEADERS) $(PROTOSOURCES) $(PROTOPY) schema.cpp

libships_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(SQLITE3_CFLAGS) $(GLOG_CFLAGS) $(PROTOBUF_CFLAGS)
libships_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(SQLITE3_CFLAGS) $(GLOG_LIBS) $(PROTOBUF_LIBS)
libships_la_SOURCES = \
  board.cpp \
  channel.cpp \
  coord.cpp \
  gamestatejson.cpp \
  grid.cpp \
  logic.cpp \
  schema.cpp \
  $(PROTOSOURCES)
noinst_HEADERS = \
  board.hpp \
  channel.hpp \
  coord.hpp \
  gamestatejson.hpp \
  grid.hpp \
  logic.hpp \
  schema.hpp \
  $(PROTOHEADERS)

shipsd_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(GFLAGS_CFLAGS) $(PROTOBUF_CFLAGS)
shipsd_LDADD = \
  $(builddir)/libships.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS) $(GFLAGS_LIBS) $(PROTOBUF_LIBS)
shipsd_SOURCES = main-gsp.cpp

ships_channel_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GFLAGS_CFLAGS) $(PROTOBUF_CFLAGS)
ships_channel_LDADD = \
  $(builddir)/libships.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GFLAGS_LIBS) $(PROTOBUF_LIBS)
ships_channel_SOURCES = main-channel.cpp

noinst_PYTHON = proto/__init__.py $(PROTOPY)

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(PROTOBUF_CFLAGS)
tests_LDADD = \
  $(builddir)/libships.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libtestutils.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS) $(GTEST_LIBS) $(PROTOBUF_LIBS)
tests_SOURCES = \
  board_tests.cpp \
  channel_tests.cpp \
  coord_tests.cpp \
  gamestatejson_tests.cpp \
  grid_tests.cpp \
  logic_tests.cpp \
  schema_tests.cpp \
  \
  testutils.cpp
check_HEADERS = \
  testutils.hpp

schema.cpp: schema_head.cpp schema.sql schema_tail.cpp
	cat $^ >$@

proto/%.pb.h proto/%.pb.cc: $(srcdir)/proto/%.proto
	protoc -I$(top_srcdir) -I$(srcdir)/proto --cpp_out=proto "$<"
proto/%_pb2.py: $(srcdir)/proto/%.proto
	protoc -I$(top_srcdir) -I$(srcdir)/proto --python_out=proto "$<"
