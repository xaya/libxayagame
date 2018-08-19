lib_LTLIBRARIES = libxayagame.la
xayagamedir = $(includedir)/xayagame

libxayagame_la_CXXFLAGS = $(GLOG_CFLAGS) $(ZMQ_CFLAGS)
libxayagame_la_LIBADD = $(GLOG_LIBS) $(ZMQ_LIBS)
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

tests_CXXFLAGS = $(GTEST_CFLAGS)
tests_LDADD = $(builddir)/libxayagame.la $(GTEST_LIBS)
tests_SOURCES = mainloop_tests.cpp
