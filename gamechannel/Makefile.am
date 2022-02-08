lib_LTLIBRARIES = libchannelcore.la libgamechannel.la
dist_bin_SCRIPTS = rpc-channel-server.py
dist_data_DATA = rpc-stubs/channel-gsp-rpc.json
gamechanneldir = $(includedir)/gamechannel
rpcstubdir = $(gamechanneldir)/rpc-stubs
protodir = $(gamechanneldir)/proto
pydir = $(pythondir)/gamechannel
pyprotodir = $(pydir)/proto

pkgconfig_DATA = channelcore.pc gamechannel.pc

RPC_STUBS = \
  rpc-stubs/channelgsprpcclient.h \
  rpc-stubs/channelgsprpcserverstub.h \
  rpc-stubs/rpcbroadcastclient.h

PROTOS = \
  proto/broadcast.proto \
  proto/metadata.proto \
  proto/testprotos.proto \
  proto/signatures.proto \
  proto/stateproof.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)
PROTOPY = $(PROTOS:.proto=_pb2.py)

EXTRA_DIST = $(PROTOS) \
  rpc-stubs/rpcbroadcast.json \
  schema.sql schema_head.cpp schema_tail.cpp

BUILT_SOURCES = $(RPC_STUBS) $(PROTOHEADERS) $(PROTOPY)
CLEANFILES = $(RPC_STUBS) $(PROTOHEADERS) $(PROTOSOURCES) $(PROTOPY) schema.cpp

libchannelcore_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(ETHUTILS_CFLAGS) $(GLOG_CFLAGS) $(PROTOBUF_CFLAGS)
libchannelcore_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(JSONCPP_LIBS) $(ETHUTILS_LIBS) $(GLOG_LIBS) $(PROTOBUF_LIBS)
libchannelcore_la_SOURCES = \
  boardrules.cpp \
  broadcast.cpp \
  channelmanager.cpp \
  channelstatejson.cpp \
  movesender.cpp \
  openchannel.cpp \
  protoversion.cpp \
  rollingstate.cpp \
  signatures.cpp \
  stateproof.cpp \
  $(PROTOSOURCES)
CHANNELCOREHEADERS = \
  boardrules.hpp \
  broadcast.hpp \
  channelmanager.hpp channelmanager.tpp \
  channelstatejson.hpp \
  movesender.hpp \
  openchannel.hpp \
  protoboard.hpp protoboard.tpp \
  protoutils.hpp protoutils.tpp \
  protoversion.hpp \
  rollingstate.hpp \
  signatures.hpp \
  stateproof.hpp

libgamechannel_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(SQLITE3_CFLAGS) $(PROTOBUF_CFLAGS)
libgamechannel_la_LIBADD = \
  $(builddir)/libchannelcore.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(SQLITE3_LIBS) $(PROTOBUF_LIBS)
libgamechannel_la_SOURCES = \
  channelgame.cpp \
  chaintochannel.cpp \
  daemon.cpp \
  database.cpp \
  gamestatejson.cpp \
  gsprpc.cpp \
  recvbroadcast.cpp \
  rpcbroadcast.cpp \
  rpcwallet.cpp \
  schema.cpp \
  syncmanager.cpp
GAMECHANNELHEADERS = \
  channelgame.hpp \
  chaintochannel.hpp \
  daemon.hpp \
  database.hpp \
  gamestatejson.hpp \
  gsprpc.hpp \
  recvbroadcast.hpp \
  rpcbroadcast.hpp \
  rpcwallet.hpp \
  schema.hpp \
  syncmanager.hpp

gamechannel_HEADERS = $(CHANNELCOREHEADERS) $(GAMECHANNELHEADERS)
rpcstub_HEADERS = $(RPC_STUBS)
proto_HEADERS = $(PROTOHEADERS)

PYTHONTESTS = \
  signatures_tests.py \
  test_rpcbroadcast.py
noinst_PYTHON = $(PYTHONTESTS)
py_PYTHON = \
  __init__.py \
  channeltest.py \
  rpcbroadcast.py \
  signatures.py
pyproto_PYTHON = proto/__init__.py $(PROTOPY)

AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(top_srcdir)

check_LTLIBRARIES = libtestutils.la
check_PROGRAMS = tests test_rpcbroadcast
TESTS = tests $(PYTHONTESTS)

libtestutils_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS)
libtestutils_la_LIBADD = \
  $(builddir)/libgamechannel.la \
  $(builddir)/libchannelcore.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(GLOG_LIBS) $(GTEST_LIBS)
libtestutils_la_SOURCES = \
  testutils.cpp
TESTUTILHEADERS = \
  testutils.hpp

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(SQLITE3_CFLAGS) $(PROTOBUF_CFLAGS)
tests_LDADD = \
  $(builddir)/libtestutils.la \
  $(builddir)/libgamechannel.la \
  $(builddir)/libchannelcore.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/xayagame/libtestutils.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GTEST_LIBS) $(SQLITE3_LIBS) $(PROTOBUF_LIBS)
tests_SOURCES = \
  broadcast_tests.cpp \
  channelgame_tests.cpp \
  channelmanager_tests.cpp \
  channelstatejson_tests.cpp \
  chaintochannel_tests.cpp \
  database_tests.cpp \
  gamestatejson_tests.cpp \
  movesender_tests.cpp \
  protoboard_tests.cpp \
  protoutils_tests.cpp \
  protoversion_tests.cpp \
  recvbroadcast_tests.cpp \
  rollingstate_tests.cpp \
  schema_tests.cpp \
  signatures_tests.cpp \
  stateproof_tests.cpp \
  syncmanager_tests.cpp \
  testgame_tests.cpp \
  \
  testgame.cpp
TESTHEADERS = \
  channelmanager_tests.hpp \
  channelstatejson_tests.hpp \
  \
  testgame.hpp

check_HEADERS = $(TESTUTILHEADERS) $(TESTHEADERS)

test_rpcbroadcast_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GFLAGS_CFLAGS) $(PROTOBUF_CFLAGS)
test_rpcbroadcast_LDADD = \
  $(builddir)/libgamechannel.la \
  $(builddir)/libchannelcore.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GFLAGS_LIBS) $(PROTOBUF_LIBS)
test_rpcbroadcast_SOURCES = test_rpcbroadcast.cpp

schema.cpp: schema_head.cpp schema.sql schema_tail.cpp
	cat $^ >$@

rpc-stubs/channelgsprpcclient.h: $(srcdir)/rpc-stubs/channel-gsp-rpc.json
	jsonrpcstub "$<" --cpp-client=ChannelGspRpcClient --cpp-client-file="$@"
rpc-stubs/channelgsprpcserverstub.h: $(srcdir)/rpc-stubs/channel-gsp-rpc.json
	jsonrpcstub "$<" \
	  --cpp-server=ChannelGspRpcServerStub \
	  --cpp-server-file="$@"

rpc-stubs/rpcbroadcastclient.h: $(srcdir)/rpc-stubs/rpcbroadcast.json
	jsonrpcstub "$<" --cpp-client=RpcBroadcastClient --cpp-client-file="$@"

proto/%.pb.h proto/%.pb.cc: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --cpp_out=$(top_builddir) "$<"
proto/%_pb2.py: $(top_srcdir)/gamechannel/proto/%.proto
	protoc -I$(top_srcdir) --python_out=$(top_builddir) "$<"
