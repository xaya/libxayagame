noinst_LTLIBRARIES = libnonfungible.la
bin_PROGRAMS = nonfungibled

libnonfungible_la_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(SQLITE_CFLAGS) $(GLOG_CFLAGS)
libnonfungible_la_LIBADD = \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(SQLITE_LIBS) $(GLOG_LIBS)
libnonfungible_la_SOURCES = \
  logic.cpp
noinst_HEADERS = \
  logic.hpp

nonfungibled_CXXFLAGS = \
  -I$(top_srcdir) \
  $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(JSONRPCSERVER_CFLAGS) \
  $(GLOG_CFLAGS) $(GFLAGS_CFLAGS)
nonfungibled_LDADD = \
  $(builddir)/libnonfungible.la \
  $(top_builddir)/xayautil/libxayautil.la \
  $(top_builddir)/xayagame/libxayagame.la \
  $(GLOG_LIBS) $(GFLAGS_LIBS)
nonfungibled_SOURCES = main.cpp
