#include "xayagame/game.hpp"

#include <glog/logging.h>

#include <cstdlib>

class RpsGame : public xaya::Game
{

public:

  RpsGame ()
    : xaya::Game("rps")
  {}

};

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);

  RpsGame game;
  game.SetZmqEndpoint ("tcp://127.0.0.1:28555");
  game.Run ();

  return EXIT_SUCCESS;
}
