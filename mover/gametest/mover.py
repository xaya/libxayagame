# Copyright (C) 2018 The Xaya developers
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
    srcdir = os.getenv ("srcdir")
    if srcdir is None:
      srcdir = "."
    moverd = os.path.join (srcdir, "..", "moverd")
    super (MoverTest, self).__init__ ("mv", moverd)

  def move (self, name, direction, steps):
    """
    Utility method to send a Mover move.
    """

    self.sendMove (name, {"d": direction, "n": steps})
