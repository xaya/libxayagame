SUBDIRS = gametest channeltest

noinst_LTLIBRARIES = libshipscore.la libships.la
bin_PROGRAMS = shipsd ships-channel

RPC_STUBS = \
  rpc-stubs/shipschannelrpcserverstub.h

PROTOS = \
  proto/boardmove.proto \
  proto/boardstate.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)
PROTOPY = $(PROTOS:.proto=_pb2.py)

EXTRA_DIST = $(PROTOS) \
  rpc-stubs/channel.json \
  schema.sql schema_head.cpp schema_tail.cpp

BUILT_SOURCES = $(RPC_STUBS) $(PROTOHEADERS) $(PROTOPY)
CLEANFILES = $(RPC_STUBS) $(PROTOHEADERS) $(PROTOSOURCES) $(PROTOPY) schema.cpp

libshipscore_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(PROTOBUF_CFLAGS)
libshipscore_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/gamechannel/libchannelcore.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS) $(PROTOBUF_LIBS)
libshipscore_la_SOURCES = \
  board.cpp \
  channel.cpp \
  coord.cpp \
  grid.cpp \
  $(PROTOSOURCES)
SHIPSCOREHEADERS = \
  board.hpp \
  channel.hpp \
  coord.hpp \
  grid.hpp \
  $(PROTOHEADERS)

libships_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(SQLITE3_CFLAGS) $(GLOG_CFLAGS) $(PROTOBUF_CFLAGS)
libships_la_LIBADD = \
  $(builddir)/libshipscore.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libchannelcore.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(SQLITE3_CFLAGS) $(GLOG_LIBS) $(PROTOBUF_LIBS)
libships_la_SOURCES = \
  gamestatejson.cpp \
  logic.cpp \
  schema.cpp
LIBSHIPSHEADERS = \
  gamestatejson.hpp \
  logic.hpp \
  schema.hpp \
  $(RPC_STUBS)

shipsd_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(GFLAGS_CFLAGS) $(PROTOBUF_CFLAGS)
shipsd_LDADD = \
  $(builddir)/libshipscore.la \
  $(builddir)/libships.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libchannelcore.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS) $(GFLAGS_LIBS) $(PROTOBUF_LIBS)
shipsd_SOURCES = main-gsp.cpp

ships_channel_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GFLAGS_CFLAGS) $(PROTOBUF_CFLAGS)
ships_channel_LDADD = \
  $(builddir)/libshipscore.la \
  $(builddir)/libships.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libchannelcore.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GFLAGS_LIBS) $(PROTOBUF_LIBS)
ships_channel_SOURCES = main-channel.cpp \
  channelrpc.cpp
CHANNELHEADERS = \
  channelrpc.hpp

noinst_HEADERS = $(SHIPSCOREHEADERS) $(LIBSHIPSHEADERS) $(CHANNELHEADERS)

noinst_PYTHON = proto/__init__.py $(PROTOPY)

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(PROTOBUF_CFLAGS)
tests_LDADD = \
  $(builddir)/libshipscore.la \
  $(builddir)/libships.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libtestutils.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libchannelcore.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(top_builddir)/gamechannel/libtestutils.la \
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

rpc-stubs/shipschannelrpcserverstub.h: $(srcdir)/rpc-stubs/channel.json
	jsonrpcstub "$<" \
	  --cpp-server=ShipsChannelRpcServerStub \
	  --cpp-server-file="$@"

proto/%.pb.h proto/%.pb.cc: $(srcdir)/proto/%.proto
	protoc -I$(top_srcdir) -I$(srcdir)/proto --cpp_out=proto "$<"
proto/%_pb2.py: $(srcdir)/proto/%.proto
	protoc -I$(top_srcdir) -I$(srcdir)/proto --python_out=proto "$<"
