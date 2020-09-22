# Copyright (C) 2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from xayagametest.testcase import XayaGameTest

import os
import os.path


class NonFungibleTest (XayaGameTest):
  """
  An integration test for the non-fungible GSP.
  """

  def __init__ (self):
    top_builddir = os.getenv ("top_builddir")
    if top_builddir is None:
      top_builddir = "../.."
    nfd = os.path.join (top_builddir, "nonfungible", "nonfungibled")
    super ().__init__ ("nf", nfd)

  def getRpc (self, method, *args, **kwargs):
    """
    Calls a custom-state RPC method and returns the data field.
    """

    return self.getCustomState ("data", method, *args, **kwargs)
