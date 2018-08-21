lib_LTLIBRARIES = libxayagame.la
xayagamedir = $(includedir)/xayagame

libxayagame_la_CXXFLAGS = $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(ZMQ_CFLAGS)
libxayagame_la_LIBADD = $(JSONCPP_LIBS) $(GLOG_LIBS) $(ZMQ_LIBS)
libxayagame_la_SOURCES = \
  game.cpp \
  mainloop.cpp \
  zmqsubscriber.cpp
xayagame_HEADERS = \
  game.hpp \
  mainloop.hpp \
  zmqsubscriber.hpp

check_PROGRAMS = tests
TESTS = tests

tests_CXXFLAGS = $(JSONCPP_CFLAGS) $(GLOG_CFLAGS) $(GTEST_CFLAGS) $(ZMQ_CFLAGS)
tests_LDADD = $(builddir)/libxayagame.la \
  $(JSONCPP_LIBS) $(GLOG_LIBS) $(GTEST_LIBS) $(ZMQ_LIBS)
tests_SOURCES = \
  mainloop_tests.cpp \
  zmqsubscriber_tests.cpp
