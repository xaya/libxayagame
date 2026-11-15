# Copyright (C) 2018-2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from xayagametest.testcase import XayaGameTest

import os
import os.path


class MoverTest (XayaGameTest):
  """
  An integration test for the Mover game.
  """

  def __init__ (self):
    builddir = os.getenv ("BUILD_DIR")
    if builddir is None:
      builddir = "../.."
    moverd = os.path.join (builddir, "mover", "moverd")
    super (MoverTest, self).__init__ ("mv", moverd)

  def gameBaseArgs (self):
    # Mover gametests run a few weird situations exercising the
    # libxayagame handling in general, not just mover logic.
    # Enable connection recovery for this.
    return [
      "--xaya_connection_check_ms=100",
      "--xaya_zmq_staleness_ms=1000",
    ]

  def move (self, name, direction, steps):
    """
    Utility method to send a Mover move.
    """

    return self.sendMove (name, {"d": direction, "n": steps})
