lib_LTLIBRARIES = libxayagame.la
xayagamedir = $(includedir)/xayagame

EXTRA_DIST = rpc-stubs/xaya.json

libxayagame_la_CXXFLAGS = \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(GLOG_CFLAGS) $(ZMQ_CFLAGS)
libxayagame_la_LIBADD = \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(GLOG_LIBS) $(ZMQ_LIBS)
libxayagame_la_SOURCES = \
  game.cpp \
  mainloop.cpp \
  uint256.cpp \
  zmqsubscriber.cpp
xayagame_HEADERS = \
  game.hpp \
  mainloop.hpp \
  uint256.hpp \
  zmqsubscriber.hpp \
  rpc-stubs/xayarpcclient.h \
  rpc-stubs/xayarpcserverstub.h

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(ZMQ_CFLAGS)
tests_LDADD = $(builddir)/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GTEST_LIBS) $(ZMQ_LIBS)
tests_SOURCES = \
  game_tests.cpp \
  mainloop_tests.cpp \
  uint256_tests.cpp \
  zmqsubscriber_tests.cpp

rpc-stubs/xayarpcclient.h: rpc-stubs/xaya.json
	jsonrpcstub "$<" --cpp-client=XayaRpcClient --cpp-client-file="$@"
rpc-stubs/xayarpcserverstub.h: rpc-stubs/xaya.json
	jsonrpcstub "$<" --cpp-server=XayaRpcServerStub --cpp-server-file="$@"
