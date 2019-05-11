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
  $(PROTOSOURCES)
noinst_HEADERS = \
  $(PROTOHEADERS)

proto/%.pb.h proto/%.pb.cc: $(srcdir)/proto/%.proto
	protoc -I$(top_srcdir) -I$(srcdir)/proto --cpp_out=proto "$<"
