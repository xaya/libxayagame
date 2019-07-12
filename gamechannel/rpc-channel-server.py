#!/usr/bin/env python
# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from gamechannel import rpcbroadcast

import argparse
import logging
import sys


desc = "JSON-RPC game-channel broadcast server"
parser = argparse.ArgumentParser (description=desc)
parser.add_argument ("--host", default="localhost",
                   help="host address where to bind the server")
parser.add_argument ("--port", type=int, required=True,
                   help="listening port for the server")
args = parser.parse_args ()

logFmt = "%(asctime)s %(name)s (%(levelname)s): %(message)s"
logHandler = logging.StreamHandler (sys.stderr)
logHandler.setFormatter (logging.Formatter (logFmt))
logger = logging.getLogger ()
logger.setLevel (logging.INFO)
logger.addHandler (logHandler)

server = rpcbroadcast.Server (args.host, args.port)
server.serve ()
