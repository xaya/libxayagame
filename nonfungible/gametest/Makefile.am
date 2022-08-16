AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(top_srcdir):$(PYTHONPATH)

TEST_LIBRARY = \
  nftest.py

REGTESTS = \
  burn.py \
  minting.py \
  reorg.py \
  sqlite_wal.py \
  transfers.py

EXTRA_DIST = $(REGTESTS) $(TEST_LIBRARY)
TESTS = $(REGTESTS)
