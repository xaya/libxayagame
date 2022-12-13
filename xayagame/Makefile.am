lib_LTLIBRARIES = libxayagame.la
bin_PROGRAMS = dumpxayadb
xayagamedir = $(includedir)/xayagame
rpcstubdir = $(xayagamedir)/rpc-stubs

pkgconfig_DATA = libxayagame.pc

EXTRA_DIST = rpc-stubs/game.json rpc-stubs/xaya.json rpc-stubs/xaya-wallet.json

RPC_STUBS = \
  rpc-stubs/gamerpcclient.h \
  rpc-stubs/gamerpcserverstub.h \
  rpc-stubs/xayarpcclient.h \
  rpc-stubs/xayarpcserverstub.h \
  rpc-stubs/xayawalletrpcclient.h \
  rpc-stubs/xayawalletrpcserverstub.h
BUILT_SOURCES = $(RPC_STUBS)
CLEANFILES = $(RPC_STUBS)

libxayagame_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(OPENSSL_CFLAGS) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(ZLIB_CFLAGS) $(CURL_CFLAGS) $(MHD_CFLAGS) \
  $(GLOG_CFLAGS) $(SQLITE3_CFLAGS) $(LMDB_CFLAGS) $(ZMQ_CFLAGS)
libxayagame_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(OPENSSL_LIBS) \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(ZLIB_LIBS) $(CURL_LIBS) $(MHD_LIBS) \
  $(GLOG_LIBS) $(SQLITE3_LIBS) $(LMDB_LIBS) $(ZMQ_LIBS) \
  -lstdc++fs
libxayagame_la_SOURCES = \
  defaultmain.cpp \
  game.cpp \
  gamelogic.cpp \
  gamerpcserver.cpp \
  heightcache.cpp \
  lmdbstorage.cpp \
  mainloop.cpp \
  pendingmoves.cpp \
  pruningqueue.cpp \
  rest.cpp \
  signatures.cpp \
  sqlitegame.cpp \
  sqliteintro.cpp \
  sqlitestorage.cpp \
  storage.cpp \
  transactionmanager.cpp \
  zmqsubscriber.cpp
xayagame_HEADERS = \
  defaultmain.hpp \
  game.hpp \
  gamelogic.hpp \
  gamerpcserver.hpp \
  heightcache.hpp \
  lmdbstorage.hpp \
  mainloop.hpp \
  pendingmoves.hpp \
  pruningqueue.hpp \
  rest.hpp \
  signatures.hpp \
  sqlitegame.hpp \
  sqliteintro.hpp sqliteintro.tpp \
  sqlitestorage.hpp \
  storage.hpp \
  transactionmanager.hpp \
  zmqsubscriber.hpp
rpcstub_HEADERS = $(RPC_STUBS)

dumpxayadb_CXXFLAGS = \
  -I$(top_srcdir) \
  $(GLOG_CFLAGS) $(SQLITE3_CFLAGS) $(GFLAGS_CFLAGS)
dumpxayadb_LDADD = \
  $(builddir)/libxayagame.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(GLOG_LIBS) $(SQLITE3_LIBS) $(GFLAGS_LIBS)
dumpxayadb_SOURCES = main-dumpdb.cpp

check_LTLIBRARIES = libtestutils.la
check_PROGRAMS = tests
TESTS = tests

libtestutils_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS)
libtestutils_la_LIBADD = \
  $(builddir)/libxayagame.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GTEST_LIBS)
libtestutils_la_SOURCES = \
  testutils.cpp
TESTUTILHEADERS = testutils.hpp

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(SQLITE3_CFLAGS) $(LMDB_CFLAGS) $(ZMQ_CFLAGS)
tests_LDADD = \
  $(builddir)/libtestutils.la \
  $(builddir)/libxayagame.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(JSONRPCSERVER_LIBS) \
  $(GLOG_LIBS) $(GTEST_LIBS) $(SQLITE3_LIBS) $(LMDB_LIBS) $(ZMQ_LIBS)
tests_SOURCES = \
  game_tests.cpp \
  gamelogic_tests.cpp \
  heightcache_tests.cpp \
  lmdbstorage_tests.cpp \
  mainloop_tests.cpp \
  pendingmoves_tests.cpp \
  pruningqueue_tests.cpp \
  rest_tests.cpp \
  signatures_tests.cpp \
  sqlitegame_tests.cpp \
  sqliteintro_tests.cpp \
  sqlitestorage_tests.cpp \
  storage_tests.cpp \
  transactionmanager_tests.cpp \
  zmqsubscriber_tests.cpp
TESTHEADERS = storage_tests.hpp

check_HEADERS = $(TESTUTILHEADERS) $(TESTHEADERS)

rpc-stubs/gamerpcclient.h: $(srcdir)/rpc-stubs/game.json
	jsonrpcstub "$<" --cpp-client=GameRpcClient --cpp-client-file="$@"
rpc-stubs/gamerpcserverstub.h: $(srcdir)/rpc-stubs/game.json
	jsonrpcstub "$<" --cpp-server=GameRpcServerStub --cpp-server-file="$@"

rpc-stubs/xayarpcclient.h: $(srcdir)/rpc-stubs/xaya.json
	jsonrpcstub "$<" --cpp-client=XayaRpcClient --cpp-client-file="$@"
rpc-stubs/xayarpcserverstub.h: $(srcdir)/rpc-stubs/xaya.json
	jsonrpcstub "$<" --cpp-server=XayaRpcServerStub --cpp-server-file="$@"

rpc-stubs/xayawalletrpcclient.h: $(srcdir)/rpc-stubs/xaya-wallet.json
	jsonrpcstub "$<" --cpp-client=XayaWalletRpcClient --cpp-client-file="$@"
rpc-stubs/xayawalletrpcserverstub.h: $(srcdir)/rpc-stubs/xaya-wallet.json
	jsonrpcstub "$<" \
	  --cpp-server=XayaWalletRpcServerStub \
	  --cpp-server-file="$@"
