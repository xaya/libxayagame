lib_LTLIBRARIES = libgamechannel.la
gamechanneldir = $(includedir)/gamechannel
rpcstubdir = $(gamechanneldir)/rpc-stubs
protodir = $(gamechanneldir)/proto
pydir = $(pythondir)/gamechannel
pyprotodir = $(pydir)/proto

RPC_STUBS = \
  rpc-stubs/channelgsprpcclient.h \
  rpc-stubs/channelgsprpcserverstub.h

PROTOS = \
  proto/metadata.proto \
  proto/protoboardtest.proto \
  proto/signatures.proto \
  proto/stateproof.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)
PROTOPY = $(PROTOS:.proto=_pb2.py)

EXTRA_DIST = $(PROTOS) \
  rpc-stubs/gsp.json \
  schema.sql schema_head.cpp schema_tail.cpp

BUILT_SOURCES = $(RPC_STUBS) $(PROTOHEADERS) $(PROTOPY)
CLEANFILES = $(RPC_STUBS) $(PROTOHEADERS) $(PROTOSOURCES) $(PROTOPY) schema.cpp

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
  channelmanager.cpp \
  chaintochannel.cpp \
  database.cpp \
  gamestatejson.cpp \
  gsprpc.cpp \
  rollingstate.cpp \
  schema.cpp \
  signatures.cpp \
  stateproof.cpp \
  $(PROTOSOURCES)
gamechannel_HEADERS = \
  boardrules.hpp \
  channelgame.hpp \
  channelmanager.hpp \
  chaintochannel.hpp \
  database.hpp \
  gamestatejson.hpp \
  gsprpc.hpp \
  movesender.hpp \
  protoboard.hpp protoboard.tpp \
  protoutils.hpp protoutils.tpp \
  rollingstate.hpp \
  schema.hpp \
  signatures.hpp \
  stateproof.hpp
rpcstub_HEADERS = $(RPC_STUBS)
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
  channelmanager_tests.cpp \
  chaintochannel_tests.cpp \
  database_tests.cpp \
  gamestatejson_tests.cpp \
  protoboard_tests.cpp \
  protoutils_tests.cpp \
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

rpc-stubs/channelgsprpcclient.h: $(srcdir)/rpc-stubs/gsp.json
	jsonrpcstub "$<" --cpp-client=ChannelGspRpcClient --cpp-client-file="$@"
rpc-stubs/channelgsprpcserverstub.h: $(srcdir)/rpc-stubs/gsp.json
	jsonrpcstub "$<" \
	  --cpp-server=ChannelGspRpcServerStub \
	  --cpp-server-file="$@"

proto/%.pb.h proto/%.pb.cc: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --cpp_out=$(top_builddir) "$<"
proto/%_pb2.py: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --python_out=$(top_builddir) "$<"
