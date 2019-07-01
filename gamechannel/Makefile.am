lib_LTLIBRARIES = libgamechannel.la
gamechanneldir = $(includedir)/gamechannel
protodir = $(gamechanneldir)/proto
pydir = $(pythondir)/gamechannel
pyprotodir = $(pydir)/proto

PROTOS = \
  proto/metadata.proto \
  proto/protoboardtest.proto \
  proto/signatures.proto \
  proto/stateproof.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)
PROTOPY = $(PROTOS:.proto=_pb2.py)

EXTRA_DIST = $(PROTOS) \
  schema.sql schema_head.cpp schema_tail.cpp

BUILT_SOURCES = $(PROTOHEADERS) $(PROTOPY)
CLEANFILES = $(PROTOHEADERS) $(PROTOSOURCES) $(PROTOPY) schema.cpp

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
  gamestatejson.cpp \
  rollingstate.cpp \
  schema.cpp \
  signatures.cpp \
  stateproof.cpp \
  $(PROTOSOURCES)
gamechannel_HEADERS = \
  boardrules.hpp \
  channelgame.hpp \
  database.hpp \
  gamestatejson.hpp \
  protoboard.hpp protoboard.tpp \
  rollingstate.hpp \
  schema.hpp \
  signatures.hpp \
  stateproof.hpp
proto_HEADERS = $(PROTOHEADERS)

PYTHONTESTS = \
  signatures_tests.py
noinst_PYTHON = $(PYTHONTESTS)
py_PYTHON = __init__.py signatures.py
pyproto_PYTHON = proto/__init__.py $(PROTOPY)

AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(srcdir)

check_PROGRAMS = tests
TESTS = tests $(PYTHONTESTS)

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
  channelgame_tests.cpp \
  database_tests.cpp \
  gamestatejson_tests.cpp \
  protoboard_tests.cpp \
  rollingstate_tests.cpp \
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

proto/%.pb.h proto/%.pb.cc: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --cpp_out=$(top_builddir) "$<"
proto/%_pb2.py: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --python_out=$(top_builddir) "$<"
