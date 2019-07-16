// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <glog/logging.h>

#include <stdexcept>
#include <iostream>

namespace
{

/**
 * Tests simple throw of an exception without further ado.
 */
void
BasicThrow ()
{
  LOG (INFO) << "Testing basic throw and catch...";

  try
    {
      LOG (INFO) << "Throwing exception...";
      throw std::runtime_error ("test exception");
      LOG (FATAL) << "Exception not thrown!";
    }
  catch (const std::exception& exc)
    {
      LOG (INFO) << "Caught exception: " << exc.what ();
    }
}

} // anonymous namespace

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);

  BasicThrow ();

  return EXIT_SUCCESS;
}
