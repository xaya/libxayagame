noinst_LTLIBRARIES = libmover.la
bin_PROGRAMS = moverd

libmover_la_CXXFLAGS = $(JSONCPP_CFLAGS) $(GLOG_CFLAGS)
libmover_la_LIBADD = $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS)
libmover_la_SOURCES = \
  logic.cpp
noinst_HEADERS = \
  logic.hpp

moverd_CXXFLAGS = $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(GLOG_CFLAGS)
moverd_LDADD = $(builddir)/libmover.la $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(GLOG_LIBS)
moverd_SOURCES = main.cpp
