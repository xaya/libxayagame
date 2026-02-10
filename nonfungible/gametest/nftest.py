# Copyright (C) 2020-2023 The Xaya developers
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
    builddir = os.getenv ("BUILD_DIR")
    if builddir is None:
      builddir = "../.."
    nfd = os.path.join (builddir, "nonfungible", "nonfungibled")
    super ().__init__ ("nf", nfd)

  def setup (self):
    super ().setup ()

    self.collectPremine ()
    sendTo = {}
    for _ in range (10):
      sendTo[self.rpc.xaya.getnewaddress ()] = 10
    self.rpc.xaya.sendmany ("", sendTo)
    self.generate (1)

  def getRpc (self, method, *args, **kwargs):
    """
    Calls a custom-state RPC method and returns the data field.
    """

    return self.getCustomState ("data", method, *args, **kwargs)
