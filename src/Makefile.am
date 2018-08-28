noinst_LTLIBRARIES = librps.la
bin_PROGRAMS = rpsd

librps_la_CXXFLAGS = $(JSONCPP_CFLAGS) $(GLOG_CFLAGS)
librps_la_LIBADD = $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS)
librps_la_SOURCES = \
  logic.cpp
noinst_HEADERS = \
  logic.hpp

rpsd_CXXFLAGS = $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(GLOG_CFLAGS)
rpsd_LDADD = $(builddir)/librps.la $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(GLOG_LIBS)
rpsd_SOURCES = rpsd.cpp
