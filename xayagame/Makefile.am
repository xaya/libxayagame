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
