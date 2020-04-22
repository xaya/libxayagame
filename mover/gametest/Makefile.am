AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(top_srcdir)

TEST_LIBRARY = \
  mover.py \
  pending.py \
  persistence.py

REGTESTS = \
  basic.py \
  bind_locally.py \
  catching_up.py \
  dupjsonkeys.py \
  getnullstate.py \
  pending-disabled.py \
  pending-onesocket.py \
  pending-twosockets.py \
  persistence-lmdb.py \
  persistence-sqlite.py \
  pruning.py \
  reorg.py \
  stopped_xayad.py \
  waitforchange.py \
  xayarpcwait.py

# Additional tests which are not run as part of "make check" but can be
# run manually.  They are resource and time consuming, so that running them
# every time is not feasible.
ADDITIONAL_TESTS = \
  lmdb_resize.py

EXTRA_DIST = $(REGTESTS) $(ADDITIONAL_TESTS) $(TEST_LIBRARY)
TESTS = $(REGTESTS)
