noinst_LTLIBRARIES = libships.la
bin_PROGRAMS = shipsd

PROTOS = \
  proto/boardmove.proto \
  proto/boardstate.proto \
  proto/winnerstatement.proto
PROTOHEADERS = $(PROTOS:.proto=.pb.h)
PROTOSOURCES = $(PROTOS:.proto=.pb.cc)

EXTRA_DIST = $(PROTOS)

BUILT_SOURCES = $(PROTOHEADERS)
CLEANFILES = $(PROTOHEADERS) $(PROTOSOURCES)

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
  coord.cpp \
  grid.cpp \
  logic.cpp \
  $(PROTOSOURCES)
noinst_HEADERS = \
  board.hpp \
  coord.hpp \
  grid.hpp \
  logic.hpp \
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
shipsd_SOURCES = main.cpp

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
  coord_tests.cpp \
  grid_tests.cpp \
  \
  testutils.cpp
check_HEADERS = \
  testutils.hpp

proto/%.pb.h proto/%.pb.cc: $(srcdir)/proto/%.proto
	protoc -I$(top_srcdir) -I$(srcdir)/proto --cpp_out=proto "$<"
