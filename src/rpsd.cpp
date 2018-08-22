#include "xayagame/game.hpp"

#include <glog/logging.h>

#include <cstdlib>

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);

  xaya::Game game("rps");
  game.SetZmqEndpoint ("tcp://127.0.0.1:28555");
  game.Run ();

  return EXIT_SUCCESS;
}
