# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from xayagametest.testcase import XayaGameTest

import os
import os.path


class ShipsTest (XayaGameTest):
  """
  An integration test for the ships on-chain GSP.
  """

  def __init__ (self):
    top_builddir = os.getenv ("top_builddir")
    if top_builddir is None:
      top_builddir = "../.."
    shipsd = os.path.join (top_builddir, "ships", "shipsd")
    super (ShipsTest, self).__init__ ("xs", shipsd)
