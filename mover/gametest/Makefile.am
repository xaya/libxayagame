AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(top_srcdir)

REGTESTS = \
  basic.py \
  catching_up.py \
  persistence-lmdb.py \
  persistence-sqlite.py \
  pruning.py \
  reorg.py \
  stopped_xayad.py

EXTRA_DIST = $(REGTESTS) mover.py persistence.py
TESTS = $(REGTESTS)
