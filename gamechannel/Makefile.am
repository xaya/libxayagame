lib_LTLIBRARIES = libgamechannel.la
dist_bin_SCRIPTS = rpc-channel-server.py
gamechanneldir = $(includedir)/gamechannel
rpcstubdir = $(gamechanneldir)/rpc-stubs
protodir = $(gamechanneldir)/proto
pydir = $(pythondir)/gamechannel
pyprotodir = $(pydir)/proto

RPC_STUBS = \
  rpc-stubs/channelgsprpcclient.h \
  rpc-stubs/channelgsprpcserverstub.h \
  rpc-stubs/rpcbroadcastclient.h

PROTOS = \
  proto/broadcast.proto \
  proto/metadata.proto \
  proto/protoboardtest.proto \
  proto/signatures.proto \
  proto/stateproof.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)
PROTOPY = $(PROTOS:.proto=_pb2.py)

EXTRA_DIST = $(PROTOS) \
  rpc-stubs/gsp.json \
  rpc-stubs/rpcbroadcast.json \
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
  broadcast.cpp \
  channelgame.cpp \
  channelmanager.cpp \
  chaintochannel.cpp \
  daemon.cpp \
  database.cpp \
  gamestatejson.cpp \
  gsprpc.cpp \
  movesender.cpp \
  openchannel.cpp \
  rollingstate.cpp \
  rpcbroadcast.cpp \
  schema.cpp \
  signatures.cpp \
  stateproof.cpp \
  $(PROTOSOURCES)
gamechannel_HEADERS = \
  boardrules.hpp \
  broadcast.hpp \
  channelgame.hpp \
  channelmanager.hpp \
  chaintochannel.hpp \
  daemon.hpp \
  database.hpp \
  gamestatejson.hpp \
  gsprpc.hpp \
  movesender.hpp \
  openchannel.hpp \
  protoboard.hpp protoboard.tpp \
  protoutils.hpp protoutils.tpp \
  rollingstate.hpp \
  rpcbroadcast.hpp \
  schema.hpp \
  signatures.hpp \
  stateproof.hpp
rpcstub_HEADERS = $(RPC_STUBS)
proto_HEADERS = $(PROTOHEADERS)

PYTHONTESTS = \
  signatures_tests.py \
  test_rpcbroadcast.py
noinst_PYTHON = $(PYTHONTESTS)
py_PYTHON = \
  __init__.py \
  rpcbroadcast.py \
  signatures.py
pyproto_PYTHON = proto/__init__.py $(PROTOPY)

AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(srcdir)

check_PROGRAMS = tests test_rpcbroadcast
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
  broadcast_tests.cpp \
  channelgame_tests.cpp \
  channelmanager_tests.cpp \
  chaintochannel_tests.cpp \
  database_tests.cpp \
  gamestatejson_tests.cpp \
  movesender_tests.cpp \
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
  channelmanager_tests.hpp \
  \
  testgame.hpp

test_rpcbroadcast_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GFLAGS_CFLAGS) $(PROTOBUF_CFLAGS)
test_rpcbroadcast_LDADD = \
  $(builddir)/libgamechannel.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GFLAGS_LIBS) $(PROTOBUF_LIBS)
test_rpcbroadcast_SOURCES = test_rpcbroadcast.cpp

schema.cpp: schema_head.cpp schema.sql schema_tail.cpp
	cat $^ >$@

rpc-stubs/channelgsprpcclient.h: $(srcdir)/rpc-stubs/gsp.json
	jsonrpcstub "$<" --cpp-client=ChannelGspRpcClient --cpp-client-file="$@"
rpc-stubs/channelgsprpcserverstub.h: $(srcdir)/rpc-stubs/gsp.json
	jsonrpcstub "$<" \
	  --cpp-server=ChannelGspRpcServerStub \
	  --cpp-server-file="$@"

rpc-stubs/rpcbroadcastclient.h: $(srcdir)/rpc-stubs/rpcbroadcast.json
	jsonrpcstub "$<" --cpp-client=RpcBroadcastClient --cpp-client-file="$@"

proto/%.pb.h proto/%.pb.cc: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --cpp_out=$(top_builddir) "$<"
proto/%_pb2.py: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --python_out=$(top_builddir) "$<"
