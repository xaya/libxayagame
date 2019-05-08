lib_LTLIBRARIES = libgamechannel.la
gamechanneldir = $(includedir)/gamechannel

PROTOS = metadata.proto signatures.proto stateproof.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)

EXTRA_DIST = $(PROTOS) \
  schema.sql schema_head.cpp schema_tail.cpp

BUILT_SOURCES = $(PROTOHEADERS)
CLEANFILES = $(PROTOHEADERS) $(PROTOSOURCES) schema.cpp

libgamechannel_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(SQLITE3_CFLAGS) $(PROTOBUF_CFLAGS)
libgamechannel_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(SQLITE3_LIBS) $(PROTOBUF_LIBS)
libgamechannel_la_SOURCES = \
  boardrules.cpp \
  channelgame.cpp \
  database.cpp \
  schema.cpp \
  signatures.cpp \
  stateproof.cpp \
  $(PROTOSOURCES)
gamechannel_HEADERS = \
  boardrules.hpp \
  channelgame.hpp \
  database.hpp \
  schema.hpp \
  signatures.hpp \
  stateproof.hpp \
  $(PROTOHEADERS)

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(SQLITE3_CFLAGS) $(PROTOBUF_CFLAGS)
tests_LDADD = \
  $(builddir)/libgamechannel.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/xayagame/libtestutils.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GTEST_LIBS) $(SQLITE3_LIBS) $(PROTOBUF_LIBS)
tests_SOURCES = \
  database_tests.cpp \
  schema_tests.cpp \
  signatures_tests.cpp \
  stateproof_tests.cpp \
  testgame_tests.cpp \
  \
  testgame.cpp
check_HEADERS = \
  testgame.hpp

schema.cpp: schema_head.cpp schema.sql schema_tail.cpp
	cat $^ >$@

%.pb.h %.pb.cc: $(srcdir)/%.proto
	protoc -I$(srcdir) --cpp_out=. "$<"
