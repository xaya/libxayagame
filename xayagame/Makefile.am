lib_LTLIBRARIES = libxayagame.la
xayagamedir = $(includedir)/xayagame

pkgconfig_DATA = libxayagame.pc

EXTRA_DIST = rpc-stubs/game.json rpc-stubs/xaya.json

RPC_STUBS = \
  rpc-stubs/gamerpcclient.h \
  rpc-stubs/gamerpcserverstub.h \
  rpc-stubs/xayarpcclient.h \
  rpc-stubs/xayarpcserverstub.h
BUILT_SOURCES = $(RPC_STUBS)
CLEANFILES = $(RPC_STUBS)

libxayagame_la_CXXFLAGS = \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(SQLITE3_CFLAGS) $(LMDB_CFLAGS) $(ZMQ_CFLAGS)
libxayagame_la_LIBADD = \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(SQLITE3_LIBS) $(LMDB_LIBS) $(ZMQ_LIBS) \
  -lstdc++fs
libxayagame_la_SOURCES = \
  defaultmain.cpp \
  game.cpp \
  gamelogic.cpp \
  gamerpcserver.cpp \
  lmdbstorage.cpp \
  mainloop.cpp \
  pruningqueue.cpp \
  sqlitegame.cpp \
  sqlitestorage.cpp \
  storage.cpp \
  transactionmanager.cpp \
  uint256.cpp \
  zmqsubscriber.cpp
xayagame_HEADERS = \
  defaultmain.hpp \
  game.hpp \
  gamelogic.hpp \
  gamerpcserver.hpp \
  lmdbstorage.hpp \
  mainloop.hpp \
  pruningqueue.hpp \
  sqlitegame.hpp \
  sqlitestorage.hpp \
  storage.hpp \
  transactionmanager.hpp \
  uint256.hpp \
  zmqsubscriber.hpp \
  \
  rpc-stubs/gamerpcclient.h \
  rpc-stubs/gamerpcserverstub.h \
  rpc-stubs/xayarpcclient.h \
  rpc-stubs/xayarpcserverstub.h

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(SQLITE3_CFLAGS) $(LMDB_CFLAGS) $(ZMQ_CFLAGS)
tests_LDADD = $(builddir)/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GTEST_LIBS) $(SQLITE3_LIBS) $(LMDB_LIBS) $(ZMQ_LIBS)
tests_SOURCES = testutils.cpp \
  game_tests.cpp \
  gamelogic_tests.cpp \
  lmdbstorage_tests.cpp \
  mainloop_tests.cpp \
  pruningqueue_tests.cpp \
  sqlitegame_tests.cpp \
  sqlitestorage_tests.cpp \
  storage_tests.cpp \
  transactionmanager_tests.cpp \
  uint256_tests.cpp \
  zmqsubscriber_tests.cpp
check_HEADERS = testutils.hpp storage_tests.hpp

rpc-stubs/gamerpcclient.h: $(srcdir)/rpc-stubs/game.json
	jsonrpcstub "$<" --cpp-client=GameRpcClient --cpp-client-file="$@"
rpc-stubs/gamerpcserverstub.h: $(srcdir)/rpc-stubs/game.json
	jsonrpcstub "$<" --cpp-server=GameRpcServerStub --cpp-server-file="$@"

rpc-stubs/xayarpcclient.h: $(srcdir)/rpc-stubs/xaya.json
	jsonrpcstub "$<" --cpp-client=XayaRpcClient --cpp-client-file="$@"
rpc-stubs/xayarpcserverstub.h: $(srcdir)/rpc-stubs/xaya.json
	jsonrpcstub "$<" --cpp-server=XayaRpcServerStub --cpp-server-file="$@"
