#!/usr/bin/env python3
# Copyright (C) 2019-2020 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests what happens if pending moves are disabled in Xaya Core.
"""

from mover import MoverTest


class PendingDisabledTest (MoverTest):

  def __init__ (self):
    super (PendingDisabledTest, self).__init__ ()
    self.zmqPending = "none"

  def run (self):
    self.expectError (-32603, ".*pending moves are not tracked.*",
                      self.getPendingState)


if __name__ == "__main__":
  PendingDisabledTest ().main ()
