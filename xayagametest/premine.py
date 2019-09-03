# Copyright (C) 2019 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Code for accessing the premine coins on regtest.  The keys to them are
publicly known, and the coins can be useful for testing.
"""

  
def collect (rpc, addr=None, logger=None):
  """
  Collects the premine coins (whose keys are publicly known on regtest)
  and sends them to the given address or a new address from the wallet if
  no address is given.  This can be used in tests to obtain a large balance
  and use it for testing purposes.
  """

  for key in ["b69iyynFSWcU54LqXisbbqZ8uTJ7Dawk3V3yhht6ykxgttqMQFjb",
              "b3fgAKVQpMj24gbuh6DiXVwCCjCbo1cWiZC2fXgWEU9nXy6sdxD5"]:
    rpc.importprivkey (key, "premine", False)
  multisig = rpc.addmultisigaddress (1,
                                     ["cRH94YMZVk4MnRwPqRVebkLWerCPJDrXGN",
                                      "ceREF8QnXPsJ2iVQ1M4emggoXiXEynm59D"],
                                     "", "legacy")
  assert multisig["address"] == "dHNvNaqcD7XPDnoRjAoyfcMpHRi5upJD7p"

  genesisHash = rpc.getblockhash (0)
  genesisBlk = rpc.getblock (genesisHash, 2)
  assert len (genesisBlk["tx"]) == 1
  genesisTx = genesisBlk["tx"][0]
  assert len (genesisTx["vout"]) == 1
  txid = genesisTx["txid"]
  vout = 0
  amount = genesisTx["vout"][vout]["value"]
  if logger is not None:
    logger.info ("Genesis transaction has value of %d CHI" % amount)

  txout = rpc.gettxout (txid, vout)
  if txout is None:
    raise AssertionError ("Premine is already spent")

  if addr is None:
    addr = rpc.getnewaddress ()
  if logger is not None:
    logger.info ("Collecting premine to address %s..." % addr)

  fee = 0.01
  inputs = [{"txid": txid, "vout": vout}]
  outputs = [{addr: amount - fee}]
  rawTx = rpc.createrawtransaction (inputs, outputs)
  signed = rpc.signrawtransactionwithwallet (rawTx)
  assert signed["complete"]
  rpc.sendrawtransaction (signed["hex"], 0)

  rpc.generatetoaddress (1, rpc.getnewaddress ())
