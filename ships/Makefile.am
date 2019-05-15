noinst_LTLIBRARIES = libships.la

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
  $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(PROTOBUF_CFLAGS)
libships_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS) $(PROTOBUF_LIBS)
libships_la_SOURCES = \
  coord.cpp \
  grid.cpp \
  $(PROTOSOURCES)
noinst_HEADERS = \
  coord.hpp \
  grid.hpp \
  $(PROTOHEADERS)

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = \
  -I$(top_srcdir) \
  $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(PROTOBUF_CFLAGS)
tests_LDADD = \
  $(builddir)/libships.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(top_builddir)/gamechannel/libgamechannel.la \
  $(GLOG_LIBS) $(GTEST_LIBS) $(PROTOBUF_LIBS)
tests_SOURCES = \
  coord_tests.cpp \
  grid_tests.cpp

proto/%.pb.h proto/%.pb.cc: $(srcdir)/proto/%.proto
	protoc -I$(top_srcdir) -I$(srcdir)/proto --cpp_out=proto "$<"
