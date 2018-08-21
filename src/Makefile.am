bin_PROGRAMS = rpsd

rpsd_CXXFLAGS = $(JSONCPP_CFLAGS) $(JSONRPCCLIENT_CFLAGS) $(GLOG_CFLAGS)
rpsd_LDADD = $(top_builddir)/xayagame/libxayagame.la \
  $(JSONCPP_LIBS) $(JSONRPCCLIENT_LIBS) $(GLOG_LIBS)
rpsd_SOURCES = rpsd.cpp
